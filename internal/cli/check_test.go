package cli

import (
	"os"
	"path/filepath"
	"testing"
)

func TestScanSQLFileDangerous(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "bad.sql")
	content := `-- migrate
DELETE FROM users;
UPDATE accounts SET balance = 0;
TRUNCATE events;
DROP TABLE old_stuff;
SELECT 1;
DELETE FROM users WHERE id = 1;
`
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
	findings, err := scanSQLFile(path)
	if err != nil {
		t.Fatal(err)
	}
	rules := map[string]bool{}
	for _, f := range findings {
		rules[f.Rule] = true
	}
	for _, want := range []string{"PGC001", "PGC002", "PGC003", "PGC004"} {
		if !rules[want] {
			t.Fatalf("missing %s in %#v", want, findings)
		}
	}
	if rules["safe"] {
		t.Fatal("unexpected")
	}
}

func TestCmdCheckSQLOK(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "ok.sql")
	if err := os.WriteFile(path, []byte("DELETE FROM users WHERE id = 1;\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	code := CmdCheckSQL(Config{Format: "text"}, []string{path})
	if code != 0 {
		t.Fatalf("code %d", code)
	}
}
