package main

import (
	"context"
	"fmt"
	"os"

	"github.com/PG-Circuit/pg-circuit/internal/cli"
)

var version = "0.1.0"

func main() {
	os.Exit(run(os.Args[1:]))
}

func run(args []string) int {
	cfg, remaining, err := cli.ParseFlags(args)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return cli.ExitUsage
	}
	if len(remaining) == 0 {
		cli.PrintUsage(os.Stderr)
		return cli.ExitUsage
	}

	cmd := remaining[0]
	cmdArgs := remaining[1:]

	if cmd == "version" || cmd == "--version" || cmd == "-V" {
		fmt.Printf("pgcircuit %s\n", version)
		return 0
	}
	if cmd == "help" || cmd == "-h" || cmd == "--help" {
		cli.PrintUsage(os.Stdout)
		return 0
	}

	ctx := context.Background()
	client, err := cli.Connect(ctx, cfg)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	defer client.Close()

	switch cmd {
	case "status":
		return cli.CmdStatus(ctx, client, cfg)
	case "runtime":
		return cli.CmdRuntime(ctx, client, cfg)
	case "blockers":
		return cli.CmdBlockers(ctx, client, cfg)
	case "events":
		return cli.CmdEvents(ctx, client, cfg, cmdArgs)
	case "doctor":
		return cli.CmdDoctor(ctx, client, cfg)
	default:
		fmt.Fprintf(os.Stderr, "unknown command %q\n", cmd)
		cli.PrintUsage(os.Stderr)
		return cli.ExitUsage
	}
}
