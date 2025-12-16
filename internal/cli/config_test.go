package cli

import (
	"os"
	"testing"
)

func TestResolveDSNPrecedence(t *testing.T) {
	t.Setenv("PGCIRCUIT_DSN", "postgres://from-pgcircuit/db")
	t.Setenv("DATABASE_URL", "postgres://from-database-url/db")

	if got := ResolveDSNForTest("postgres://from-flag/db"); got != "postgres://from-flag/db" {
		t.Fatalf("flag should win, got %q", got)
	}
	if got := ResolveDSNForTest(""); got != "postgres://from-pgcircuit/db" {
		t.Fatalf("PGCIRCUIT_DSN should win over DATABASE_URL, got %q", got)
	}

	os.Unsetenv("PGCIRCUIT_DSN")
	if got := ResolveDSNForTest(""); got != "postgres://from-database-url/db" {
		t.Fatalf("DATABASE_URL fallback, got %q", got)
	}
}

func TestSanitizeDSN(t *testing.T) {
	got := SanitizeDSN("postgres://alice:s3cret@db.example:5432/prod")
	if got != "postgres://alice@db.example/prod" {
		t.Fatalf("unexpected sanitize: %q", got)
	}
	if testing.Verbose() {
		t.Log(got)
	}
}

func TestSupportedServer(t *testing.T) {
	cases := map[string]bool{
		"16.4":     true,
		"17.11":    true,
		"18.0":     true,
		"15.8":     false,
		"19.0":     false,
		"17.11 (Homebrew)": true,
	}
	for in, want := range cases {
		// supportedServer only checks prefix before space... "17.11 (Homebrew)" has prefix 17.
		got := supportedServer(in)
		if got != want {
			t.Fatalf("%q: got %v want %v", in, got, want)
		}
	}
}

func TestParseFlagsFormat(t *testing.T) {
	cfg, rem, err := ParseFlags([]string{"--format", "json", "status"})
	if err != nil {
		t.Fatal(err)
	}
	if cfg.Format != "json" || len(rem) != 1 || rem[0] != "status" {
		t.Fatalf("parse failed: %+v %v", cfg, rem)
	}
	_, _, err = ParseFlags([]string{"--format", "yaml", "status"})
	if err == nil {
		t.Fatal("expected format error")
	}
	_, _, err = ParseFlags([]string{"--format", "sarif", "status"})
	if err == nil {
		t.Fatal("expected sarif format error for Community CLI")
	}
}
