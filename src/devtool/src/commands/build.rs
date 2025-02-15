use crate::commands::clicommand::CliCommand;
use crate::commands::utils::cmake_version;
use anyhow::anyhow;
use clap::Parser;
use std::process::{ExitCode, Stdio};
use std::time::Instant;

/// Build command
#[derive(Debug, Parser)]
pub struct BuildCommand {
    /// Preset to use, for example, windows-x64-debug (check CMakePresets.json)
    #[arg(short, long)]
    preset: String,

    /// Verbose output
    #[arg(default_value_t = false, short, long)]
    verbose: bool,
}

impl CliCommand for BuildCommand {
    fn run(self) -> anyhow::Result<ExitCode> {
        let cmake_version = cmake_version()?;

        if cmake_version.as_str() < "3.26" {
            return Err(anyhow!("CMake must be version 3.26 or newer"));
        }

        let stdout_output = if self.verbose {
            Stdio::inherit()
        } else {
            Stdio::piped()
        };
        let stderr_output = if self.verbose {
            Stdio::inherit()
        } else {
            Stdio::piped()
        };

        let start = Instant::now();

        let cmake_command = std::process::Command::new("cmake")
            .arg("--build")
            .arg(format!("build/{}", self.preset))
            .stdout(stdout_output)
            .stderr(stderr_output)
            .stdin(Stdio::null())
            .output()?;

        if cmake_command.status.success() {
            let duration = start.elapsed();
            tracing::info!("Build finished in {:.2}", duration.as_secs_f32());
        } else {
            let error = String::from_utf8_lossy(&cmake_command.stdout);
            let extra_string = if self.verbose {
                String::new()
            } else {
                format!("\n{}", error)
            };
            tracing::error!("Build Failed{}", extra_string);
        }

        Ok(if cmake_command.status.success() {
            ExitCode::SUCCESS
        } else {
            ExitCode::FAILURE
        })
    }
}
