package cli

import (
	"context"
	"fmt"
	"os"
	"strings"
	"time"
)

// CmdBreakGlass prints a ticketed session override snippet and checks mode.
// Community has no policy_exceptions table — prefer SET LOCAL in a short transaction.
//
//	pgcircuit break-glass print --reason INC-123 --ttl 15m
//	pgcircuit break-glass check
func CmdBreakGlass(ctx context.Context, c *Client, cfg Config, args []string) int {
	if len(args) == 0 {
		fmt.Fprintln(os.Stderr, `usage: pgcircuit break-glass print|check

print   Emit a ticketed SET LOCAL snippet (does not change the database)
check   Show current mode; warn if observe (protection relaxed)`)
		return ExitUsage
	}
	switch args[0] {
	case "print":
		return breakGlassPrint(args[1:])
	case "check":
		return breakGlassCheck(ctx, c, cfg)
	default:
		fmt.Fprintf(os.Stderr, "unknown break-glass subcommand %q\n", args[0])
		return ExitUsage
	}
}

func breakGlassPrint(args []string) int {
	reason := ""
	ttl := 15 * time.Minute
	for i := 0; i < len(args); i++ {
		a := args[i]
		switch {
		case a == "--reason" && i+1 < len(args):
			i++
			reason = args[i]
		case strings.HasPrefix(a, "--reason="):
			reason = strings.TrimPrefix(a, "--reason=")
		case a == "--ttl" && i+1 < len(args):
			i++
			d, err := time.ParseDuration(args[i])
			if err != nil || d < time.Minute {
				fmt.Fprintln(os.Stderr, "error: --ttl must be >= 1m")
				return ExitUsage
			}
			ttl = d
		case strings.HasPrefix(a, "--ttl="):
			d, err := time.ParseDuration(strings.TrimPrefix(a, "--ttl="))
			if err != nil || d < time.Minute {
				fmt.Fprintln(os.Stderr, "error: --ttl must be >= 1m")
				return ExitUsage
			}
			ttl = d
		default:
			fmt.Fprintf(os.Stderr, "unknown print argument %q\n", a)
			return ExitUsage
		}
	}
	if strings.TrimSpace(reason) == "" {
		fmt.Fprintln(os.Stderr, "error: --reason is required (ticket / incident id)")
		return ExitUsage
	}

	fmt.Printf(`-- PG Circuit Community break-glass
-- Reason: %s
-- Window: keep this transaction under %s, then COMMIT and return to enforce
-- Prefer narrowing the SQL over relaxing mode. Pro adds TTL policy exceptions.

BEGIN;
-- Ticket: %s
SET LOCAL pg_circuit.mode = 'observe';
-- … approved DDL / DML only …
COMMIT;

-- Afterward:
--   pgcircuit events
--   pgcircuit break-glass check
--   SHOW pg_circuit.mode;   -- should be enforce (or warn during rollout)
`, reason, ttl, reason)
	return 0
}

func breakGlassCheck(ctx context.Context, c *Client, cfg Config) int {
	status, err := fetchStatus(ctx, c)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	relaxed := strings.EqualFold(status.Mode, "observe")
	out := map[string]any{
		"mode":    status.Mode,
		"enabled": status.Enabled,
		"relaxed": relaxed,
		"hint":    "observe disables ALLOW/WARN/BLOCK decisions — return to warn or enforce after the change window",
	}
	if cfg.Format == "json" {
		code := 0
		if relaxed {
			code = 1
		}
		_ = printJSON(out)
		return code
	}
	fmt.Println("PG Circuit break-glass check")
	fmt.Println()
	fmt.Printf("Mode:     %s\n", status.Mode)
	fmt.Printf("Enabled:  %v\n", status.Enabled)
	if relaxed {
		fmt.Println()
		fmt.Println("!! mode=observe — protection is relaxed. Return to warn/enforce after the ticketed window.")
		return 1
	}
	fmt.Println()
	fmt.Println("OK: mode is not observe.")
	return 0
}
