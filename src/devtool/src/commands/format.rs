/// /// Filename: format.rs
/// Author: Alan Ramirez
/// Date: 2024-12-25
/// Brief: Format subcommand
use crate::commands::clicommand::CliCommand;
use crate::commands::utils::get_project_files;
use anyhow::anyhow;
use clap::Parser;
use std::process::{Command, ExitCode, Stdio};
use tracing::{error, info};

/// Format command
#[derive(Debug, Parser)]
pub struct FormatCommand {
    /// Verbose output
    #[arg(default_value_t = false, short, long)]
    verbose: bool,

    /// Format all files. If not set, only the files that are tracked by git an
    /// have been modified will be formatted
    #[arg(default_value_t = false, short, long)]
    all_files: bool,

    /// Check only, do not format files
    #[arg(default_value_t = false, short, long)]
    check: bool,
}

impl FormatCommand {
    fn create_stdio(&self) -> (Stdio, Stdio) {
        if self.verbose {
            (Stdio::inherit(), Stdio::inherit())
        } else {
            (Stdio::piped(), Stdio::piped())
        }
    }

    fn clang_format(&self) -> anyhow::Result<ExitCode> {
        let (stdout_output, stderr_output) = self.create_stdio();

        info!("Formatting project files");

        // Exclude sources in third_party
        let files = get_project_files(vec!["*.c", "*.cpp", "*.h", "*.hpp"], !self.all_files)?
            .into_iter()
            .filter(|f| !f.starts_with("third_party"))
            .collect::<Vec<_>>();

        if files.is_empty() {
            info!("No files to format");
            return Ok(ExitCode::SUCCESS);
        }

        let check_flags = if self.check {
            vec!["--dry-run", "--Werror"]
        } else {
            vec![]
        };

        let status = Command::new("clang-format")
            .args(["-i"])
            .args(files)
            .args(check_flags)
            .stdout(stdout_output)
            .stderr(stderr_output)
            .output();

        if let Err(e) = &status {
            if let std::io::ErrorKind::NotFound = e.kind() {
                let error_message = "clang-format not found";
                return Err(anyhow!(error_message));
            }

            return Err(anyhow!("Error running clang-format: {}", e));
        }

        let output = status?;

        if !output.status.success() {
            // If stdout is piped, print the output
            let output = output.stderr;
            let output = String::from_utf8_lossy(&output);
            if !output.is_empty() {
                error!("{}", output);
            }
            return Ok(ExitCode::FAILURE);
        }

        Ok(ExitCode::SUCCESS)
    }

    fn rustfmt(&self) -> anyhow::Result<()> {
        // Move to the devtool directory
        let devtool_dir = std::env::current_dir()?.join("src/devtool");

        let (stdout_output, stderr_output) = self.create_stdio();

        let command = Command::new("cargo")
            .current_dir(devtool_dir)
            .arg("fmt")
            .stdout(stdout_output)
            .stderr(stderr_output)
            .status()?;

        if !command.success() {
            let error_message = "cargo fmt failed";
            error!("{}", error_message);
            return Err(anyhow!(error_message));
        }

        Ok(())
    }
}

impl CliCommand for FormatCommand {
    fn run(self) -> anyhow::Result<ExitCode> {
        if let Ok(exit_code) = self.clang_format() {
            return Ok(exit_code);
        }

        self.rustfmt()?;

        info!("Project formatted successfully");
        Ok(ExitCode::SUCCESS)
    }
}
