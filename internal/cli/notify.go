package cli

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"
)

// NotifyPayload is POSTed to the webhook URL for each new matching event.
type NotifyPayload struct {
	Source        string  `json:"source"`
	Edition       string  `json:"edition"`
	Text          string  `json:"text"` // Slack-friendly summary
	EventID       int64   `json:"event_id"`
	EventTime     any     `json:"event_time"`
	PID           int32   `json:"pid"`
	DatabaseName  *string `json:"database_name,omitempty"`
	UserName      *string `json:"user_name,omitempty"`
	OperationType string  `json:"operation_type"`
	RelationName  *string `json:"relation_name,omitempty"`
	RiskScore     int32   `json:"risk_score"`
	RiskLevel     string  `json:"risk_level"`
	RuntimeMode   string  `json:"runtime_mode"`
	Decision      string  `json:"decision"`
	RuleIDs       *string `json:"rule_ids,omitempty"`
	Fingerprint   *string `json:"fingerprint,omitempty"`
}

// CmdNotify watches pg_circuit_events() and POSTs webhooks for new decisions.
//
//	pgcircuit notify --url https://hooks.example/… 
//	pgcircuit notify --url "$URL" --interval 5s --decisions BLOCK
//	PGCIRCUIT_WEBHOOK_URL=… pgcircuit notify
func CmdNotify(ctx context.Context, c *Client, cfg Config, args []string) int {
	urlStr := strings.TrimSpace(os.Getenv("PGCIRCUIT_WEBHOOK_URL"))
	interval := 5 * time.Second
	decisions := map[string]bool{"BLOCK": true}
	allowPrivate := false
	catchUp := false
	dryRun := false

	for i := 0; i < len(args); i++ {
		a := args[i]
		switch {
		case a == "--url" && i+1 < len(args):
			i++
			urlStr = args[i]
		case strings.HasPrefix(a, "--url="):
			urlStr = strings.TrimPrefix(a, "--url=")
		case a == "--interval" && i+1 < len(args):
			i++
			d, err := time.ParseDuration(args[i])
			if err != nil || d < time.Second {
				fmt.Fprintf(os.Stderr, "error: invalid --interval (min 1s)\n")
				return ExitUsage
			}
			interval = d
		case strings.HasPrefix(a, "--interval="):
			d, err := time.ParseDuration(strings.TrimPrefix(a, "--interval="))
			if err != nil || d < time.Second {
				fmt.Fprintf(os.Stderr, "error: invalid --interval (min 1s)\n")
				return ExitUsage
			}
			interval = d
		case a == "--decisions" && i+1 < len(args):
			i++
			decisions = parseDecisions(args[i])
			if len(decisions) == 0 {
				fmt.Fprintf(os.Stderr, "error: --decisions must include BLOCK and/or WARN\n")
				return ExitUsage
			}
		case strings.HasPrefix(a, "--decisions="):
			decisions = parseDecisions(strings.TrimPrefix(a, "--decisions="))
			if len(decisions) == 0 {
				fmt.Fprintf(os.Stderr, "error: --decisions must include BLOCK and/or WARN\n")
				return ExitUsage
			}
		case a == "--allow-private":
			allowPrivate = true
		case a == "--catch-up":
			catchUp = true
		case a == "--dry-run":
			dryRun = true
		default:
			fmt.Fprintf(os.Stderr, "unknown notify argument %q\n", a)
			return ExitUsage
		}
	}

	if strings.TrimSpace(urlStr) == "" && !dryRun {
		fmt.Fprintf(os.Stderr, "error: webhook URL required (--url or PGCIRCUIT_WEBHOOK_URL)\n")
		return ExitUsage
	}
	if !dryRun {
		if err := ValidateEgressURL(urlStr, allowPrivate); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			return 1
		}
	}

	seen := map[int64]struct{}{}
	events, err := fetchEvents(ctx, c)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	for _, e := range events {
		seen[e.EventID] = struct{}{}
		if catchUp && decisions[strings.ToUpper(e.Decision)] {
			if err := deliverNotify(ctx, urlStr, e, dryRun); err != nil {
				fmt.Fprintf(os.Stderr, "error: webhook: %v\n", err)
				return 1
			}
		}
	}

	want := make([]string, 0, len(decisions))
	for d := range decisions {
		want = append(want, d)
	}
	fmt.Fprintf(os.Stderr, "pgcircuit notify watching for %s every %s (seeded %d events)\n",
		strings.Join(want, ","), interval, len(seen))

	ctx, stop := signal.NotifyContext(ctx, os.Interrupt, syscall.SIGTERM)
	defer stop()

	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			fmt.Fprintln(os.Stderr, "pgcircuit notify stopped")
			return 0
		case <-ticker.C:
			events, err := fetchEvents(ctx, c)
			if err != nil {
				fmt.Fprintf(os.Stderr, "warn: poll events: %v\n", err)
				continue
			}
			// Events are newest-first or arbitrary; scan all for unseen IDs.
			for _, e := range events {
				if _, ok := seen[e.EventID]; ok {
					continue
				}
				seen[e.EventID] = struct{}{}
				if !decisions[strings.ToUpper(e.Decision)] {
					continue
				}
				if err := deliverNotify(ctx, urlStr, e, dryRun); err != nil {
					fmt.Fprintf(os.Stderr, "warn: webhook: %v\n", err)
					continue
				}
				fmt.Fprintf(os.Stderr, "delivered event_id=%d decision=%s op=%s\n",
					e.EventID, e.Decision, e.OperationType)
			}
		}
	}
}

func parseDecisions(raw string) map[string]bool {
	out := map[string]bool{}
	for _, part := range strings.Split(raw, ",") {
		p := strings.ToUpper(strings.TrimSpace(part))
		if p == "BLOCK" || p == "WARN" {
			out[p] = true
		}
	}
	return out
}

func deliverNotify(ctx context.Context, urlStr string, e EventRow, dryRun bool) error {
	payload := NotifyPayload{
		Source:        "pg-circuit",
		Edition:       "community",
		Text:          formatNotifyText(e),
		EventID:       e.EventID,
		EventTime:     e.EventTime,
		PID:           e.PID,
		DatabaseName:  e.DatabaseName,
		UserName:      e.UserName,
		OperationType: e.OperationType,
		RelationName:  e.RelationName,
		RiskScore:     e.RiskScore,
		RiskLevel:     e.RiskLevel,
		RuntimeMode:   e.RuntimeMode,
		Decision:      e.Decision,
		RuleIDs:       e.RuleIDs,
		Fingerprint:   e.Fingerprint,
	}
	body, err := json.Marshal(payload)
	if err != nil {
		return err
	}
	if dryRun {
		fmt.Printf("%s\n", body)
		return nil
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, urlStr, bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("User-Agent", "pgcircuit-notify/0.1")

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	_, _ = io.Copy(io.Discard, resp.Body)
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("HTTP %d", resp.StatusCode)
	}
	return nil
}

func formatNotifyText(e EventRow) string {
	rel := "-"
	if e.RelationName != nil {
		rel = *e.RelationName
	}
	rules := ""
	if e.RuleIDs != nil {
		rules = *e.RuleIDs
	}
	return fmt.Sprintf("PG Circuit %s: %s rel=%s score=%d rules=%s",
		strings.ToUpper(e.Decision), e.OperationType, rel, e.RiskScore, rules)
}
