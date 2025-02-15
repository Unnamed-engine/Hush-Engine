use crate::commands::clicommand::CliCommand;
use crate::commands::utils::cmake_version;
use anyhow::anyhow;
use clap::Parser;
use std::process::{ExitCode, Stdio};

/// Configure command
#[derive(Debug, Parser)]
pub struct ConfigureCommand {
    /// Preset to use, for example, windows-x64-debug (check CMakePresets.json)
    /// If this option is not set, the command will list all available presets
    /// and exit
    #[arg(short, long)]
    preset: Option<String>,

    /// Verbose output
    #[arg(default_value_t = false, short, long)]
    verbose: bool,
}

impl ConfigureCommand {
    fn list_presets(&self) -> anyhow::Result<()> {
        let cmake_command = std::process::Command::new("cmake")
            .arg("--list-presets")
            .output()?;

        if cmake_command.status.success() {
            let output = String::from_utf8_lossy(&cmake_command.stdout);
            println!("{}", output);
        } else {
            let error = String::from_utf8_lossy(&cmake_command.stdout);
            eprintln!("Error listing presets: {}", error);
        }

        Ok(())
    }
}

impl CliCommand for ConfigureCommand {
    fn run(self) -> anyhow::Result<ExitCode> {
        let cmake_version = cmake_version()?;

        if cmake_version.as_str() < "3.26" {
            return Err(anyhow!("CMake must be version 3.26 or newer"));
        }

        let preset = if let Some(preset) = &self.preset {
            preset
        } else {
            self.list_presets()?;
            return Ok(ExitCode::SUCCESS);
        };

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

        let cmake_command = std::process::Command::new("cmake")
            .arg("--preset")
            .arg(&preset)
            .arg("-S")
            .arg(".")
            .arg("-B")
            .arg(format!("build/{}", preset))
            .stdout(stdout_output)
            .stderr(stderr_output)
            .stdin(Stdio::null())
            .output()?;

        if cmake_command.status.success() {
            tracing::info!("Configuring finished");
        } else {
            let error = String::from_utf8_lossy(&cmake_command.stdout);
            let extra_string = if self.verbose {
                String::new()
            } else {
                format!("\n{}", error)
            };
            tracing::error!("Configuring Failed{}", extra_string);
        }

        Ok(if cmake_command.status.success() {
            ExitCode::SUCCESS
        } else {
            ExitCode::FAILURE
        })
    }
}
