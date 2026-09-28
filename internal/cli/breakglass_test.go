package cli

import (
	"strings"
	"testing"
)

func TestBreakGlassPrintRequiresReason(t *testing.T) {
	code := breakGlassPrint(nil)
	if code != ExitUsage {
		t.Fatalf("want usage, got %d", code)
	}
}

func TestBreakGlassPrintOK(t *testing.T) {
	// ensure it returns 0; output goes to stdout
	code := breakGlassPrint([]string{"--reason", "INC-1", "--ttl", "10m"})
	if code != 0 {
		t.Fatalf("code %d", code)
	}
}

func TestFormatNotifyStillOK(t *testing.T) {
	// keep package test suite green if notify helpers move
	if !strings.Contains(formatNotifyText(EventRow{Decision: "BLOCK", OperationType: "x"}), "BLOCK") {
		t.Fatal("notify text")
	}
}
