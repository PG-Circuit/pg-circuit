package cli

import (
	"fmt"
	"net"
	"net/url"
	"strings"
)

// ValidateEgressURL rejects non-http(s) schemes and destinations that resolve
// to loopback, link-local, private, or metadata addresses (SSRF hardening).
// Pass allowPrivate=true for local development (e.g. http://127.0.0.1:9000).
func ValidateEgressURL(raw string, allowPrivate bool) error {
	raw = strings.TrimSpace(raw)
	if raw == "" {
		return fmt.Errorf("url required")
	}
	u, err := url.Parse(raw)
	if err != nil {
		return fmt.Errorf("invalid url")
	}
	scheme := strings.ToLower(u.Scheme)
	if scheme != "https" && scheme != "http" {
		return fmt.Errorf("url scheme must be http or https")
	}
	host := u.Hostname()
	if host == "" {
		return fmt.Errorf("url host required")
	}
	if allowPrivate {
		return nil
	}

	lower := strings.ToLower(host)
	switch lower {
	case "localhost", "metadata.google.internal", "metadata",
		"kubernetes.default", "kubernetes.default.svc":
		return fmt.Errorf("url host is blocked (use --allow-private for local testing)")
	}
	if strings.HasSuffix(lower, ".internal") || strings.HasSuffix(lower, ".local") {
		return fmt.Errorf("url host is blocked (use --allow-private for local testing)")
	}
	if ip := net.ParseIP(host); ip != nil {
		if isBlockedIP(ip) {
			return fmt.Errorf("url address is blocked (use --allow-private for local testing)")
		}
		return nil
	}
	ips, err := net.LookupIP(host)
	if err != nil {
		return fmt.Errorf("url host lookup failed: %w", err)
	}
	if len(ips) == 0 {
		return fmt.Errorf("url host has no addresses")
	}
	for _, ip := range ips {
		if isBlockedIP(ip) {
			return fmt.Errorf("url resolves to a blocked address (use --allow-private for local testing)")
		}
	}
	return nil
}

func isBlockedIP(ip net.IP) bool {
	if ip == nil {
		return true
	}
	if ip.IsLoopback() || ip.IsLinkLocalUnicast() || ip.IsLinkLocalMulticast() ||
		ip.IsMulticast() || ip.IsUnspecified() || ip.IsPrivate() {
		return true
	}
	if ip4 := ip.To4(); ip4 != nil {
		if ip4[0] == 169 && ip4[1] == 254 {
			return true
		}
		if ip4[0] == 100 && ip4[1] >= 64 && ip4[1] <= 127 {
			return true
		}
	}
	return false
}
