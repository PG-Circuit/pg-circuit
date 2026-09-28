package cli

import (
	"strings"
	"testing"
)

func TestValidateEgressURLPublicHTTPS(t *testing.T) {
	// Use a well-known public host; skip soft-fail if DNS is unavailable.
	err := ValidateEgressURL("https://example.com/hooks/pg", false)
	if err != nil && strings.Contains(err.Error(), "lookup failed") {
		t.Skip(err)
	}
	if err != nil {
		t.Fatal(err)
	}
}

func TestValidateEgressURLBlocksPrivate(t *testing.T) {
	cases := []string{
		"http://127.0.0.1:9000/hook",
		"http://localhost/hook",
		"http://10.0.0.5/x",
		"http://192.168.1.1/x",
		"ftp://example.com/x",
		"",
	}
	for _, u := range cases {
		if err := ValidateEgressURL(u, false); err == nil {
			t.Fatalf("expected block for %q", u)
		}
	}
}

func TestValidateEgressURLAllowPrivate(t *testing.T) {
	if err := ValidateEgressURL("http://127.0.0.1:9000/hook", true); err != nil {
		t.Fatal(err)
	}
}

func TestParseDecisions(t *testing.T) {
	got := parseDecisions("block, WARN")
	if !got["BLOCK"] || !got["WARN"] || len(got) != 2 {
		t.Fatalf("got %#v", got)
	}
	if len(parseDecisions("allow")) != 0 {
		t.Fatal("expected empty")
	}
}

func TestFormatNotifyText(t *testing.T) {
	rel := "public.users"
	rules := "PGC001"
	got := formatNotifyText(EventRow{
		Decision:      "BLOCK",
		OperationType: "delete_no_where",
		RelationName:  &rel,
		RiskScore:     95,
		RuleIDs:       &rules,
	})
	if !strings.Contains(got, "BLOCK") || !strings.Contains(got, "public.users") {
		t.Fatalf("unexpected text: %q", got)
	}
}
