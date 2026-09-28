package cli

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

// Static migration danger patterns for Community CI (warn-first).
// Not a full SQL parser — high-signal heuristics only. Pro preflight is deeper.

type CheckFinding struct {
	File    string `json:"file"`
	Line    int    `json:"line"`
	Rule    string `json:"rule"`
	Message string `json:"message"`
	Snippet string `json:"snippet"`
}

type CheckReport struct {
	OK       bool           `json:"ok"`
	Files    int            `json:"files"`
	Findings []CheckFinding `json:"findings"`
}

var (
	reDeleteNoWhere = regexp.MustCompile(`(?i)^\s*DELETE\s+FROM\s+[^\s;]+(\s+AS\s+\w+)?\s*;?\s*$`)
	reUpdateNoWhere = regexp.MustCompile(`(?i)^\s*UPDATE\s+[^\s;]+(\s+AS\s+\w+)?\s+SET\b[^;]*;\s*$`)
	reHasWhere      = regexp.MustCompile(`(?i)\bWHERE\b`)
	reTruncate      = regexp.MustCompile(`(?i)^\s*TRUNCATE\b`)
	reDropTable     = regexp.MustCompile(`(?i)^\s*DROP\s+TABLE\b`)
	reDropDatabase  = regexp.MustCompile(`(?i)^\s*DROP\s+DATABASE\b`)
	reDropSchema    = regexp.MustCompile(`(?i)^\s*DROP\s+SCHEMA\b`)
)

// CmdCheckSQL scans SQL files for high-risk shapes (Community CI warn gate).
//
//	pgcircuit check-sql migrations/*.sql
//	pgcircuit check-sql --fail-on-findings path/to/file.sql
func CmdCheckSQL(cfg Config, args []string) int {
	failOnFindings := false
	var files []string
	for _, a := range args {
		switch {
		case a == "--fail-on-findings":
			failOnFindings = true
		case strings.HasPrefix(a, "-"):
			fmt.Fprintf(os.Stderr, "unknown check-sql argument %q\n", a)
			return ExitUsage
		default:
			matches, err := filepath.Glob(a)
			if err != nil {
				fmt.Fprintf(os.Stderr, "error: glob %q: %v\n", a, err)
				return 1
			}
			if len(matches) == 0 {
				// literal path
				files = append(files, a)
			} else {
				files = append(files, matches...)
			}
		}
	}
	if len(files) == 0 {
		fmt.Fprintln(os.Stderr, "usage: pgcircuit check-sql [--fail-on-findings] <file.sql|glob>…")
		return ExitUsage
	}

	report := CheckReport{Findings: []CheckFinding{}}
	seen := map[string]struct{}{}
	for _, f := range files {
		if _, ok := seen[f]; ok {
			continue
		}
		seen[f] = struct{}{}
		st, err := os.Stat(f)
		if err != nil || st.IsDir() {
			fmt.Fprintf(os.Stderr, "warn: skip %q\n", f)
			continue
		}
		report.Files++
		findings, err := scanSQLFile(f)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error: read %q: %v\n", f, err)
			return 1
		}
		report.Findings = append(report.Findings, findings...)
	}
	report.OK = len(report.Findings) == 0

	if cfg.Format == "json" {
		code := 0
		if failOnFindings && !report.OK {
			code = 1
		}
		_ = printJSON(report)
		return code
	}

	if report.OK {
		fmt.Printf("check-sql: ok (%d file(s), no high-risk shapes)\n", report.Files)
		return 0
	}
	for _, f := range report.Findings {
		fmt.Printf("%s:%d: %s %s\n  %s\n", f.File, f.Line, f.Rule, f.Message, f.Snippet)
	}
	fmt.Printf("\ncheck-sql: %d finding(s) in %d file(s)\n", len(report.Findings), report.Files)
	fmt.Println("Community gate is warn-first. Use --fail-on-findings to fail CI.")
	fmt.Println("Pro preflight adds runtime/policy assess: https://pgcircuit.com/docs/cli")
	if failOnFindings {
		return 1
	}
	return 0
}

func scanSQLFile(path string) ([]CheckFinding, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	var findings []CheckFinding
	sc := bufio.NewScanner(f)
	// large migration lines
	buf := make([]byte, 0, 64*1024)
	sc.Buffer(buf, 1024*1024)

	lineNo := 0
	var stmt strings.Builder
	stmtStart := 0

	flush := func() {
		raw := strings.TrimSpace(stmt.String())
		stmt.Reset()
		if raw == "" || strings.HasPrefix(raw, "--") {
			return
		}
		// strip line comments roughly
		cleaned := stripSQLLineComments(raw)
		if cleaned == "" {
			return
		}
		ln := stmtStart
		if ln == 0 {
			ln = lineNo
		}
		snippet := cleaned
		if len(snippet) > 120 {
			snippet = snippet[:117] + "..."
		}
		switch {
		case reDropDatabase.MatchString(cleaned):
			findings = append(findings, CheckFinding{path, ln, "PGC005", "DROP DATABASE", snippet})
		case reDropSchema.MatchString(cleaned):
			findings = append(findings, CheckFinding{path, ln, "PGC012", "DROP SCHEMA", snippet})
		case reDropTable.MatchString(cleaned):
			findings = append(findings, CheckFinding{path, ln, "PGC004", "DROP TABLE", snippet})
		case reTruncate.MatchString(cleaned):
			findings = append(findings, CheckFinding{path, ln, "PGC003", "TRUNCATE", snippet})
		case reDeleteNoWhere.MatchString(cleaned) && !reHasWhere.MatchString(cleaned):
			findings = append(findings, CheckFinding{path, ln, "PGC001", "DELETE without WHERE", snippet})
		case strings.HasPrefix(strings.ToUpper(strings.TrimSpace(cleaned)), "UPDATE ") &&
			reUpdateNoWhere.MatchString(cleaned) && !reHasWhere.MatchString(cleaned):
			findings = append(findings, CheckFinding{path, ln, "PGC002", "UPDATE without WHERE", snippet})
		}
	}

	for sc.Scan() {
		lineNo++
		line := sc.Text()
		trim := strings.TrimSpace(line)
		if stmt.Len() == 0 && (trim == "" || strings.HasPrefix(trim, "--")) {
			continue
		}
		if stmt.Len() == 0 {
			stmtStart = lineNo
		}
		if stmt.Len() > 0 {
			stmt.WriteByte('\n')
		}
		stmt.WriteString(line)
		if strings.Contains(line, ";") {
			flush()
		}
	}
	if stmt.Len() > 0 {
		flush()
	}
	return findings, sc.Err()
}

func stripSQLLineComments(s string) string {
	var b strings.Builder
	for _, line := range strings.Split(s, "\n") {
		if i := strings.Index(line, "--"); i >= 0 {
			line = line[:i]
		}
		b.WriteString(line)
		b.WriteByte(' ')
	}
	return strings.TrimSpace(b.String())
}
