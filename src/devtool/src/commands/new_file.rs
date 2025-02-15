use crate::commands::clicommand::CliCommand;
use crate::commands::utils::git_username;
use anyhow::anyhow;
use clap::Parser;
use lazy_static::lazy_static;
use minijinja::context;
use std::collections::HashMap;
use std::fs::File;
use std::io::Write;
use std::path::PathBuf;
use std::process::ExitCode;
use tracing::{error, info};

/// Build command
#[derive(Debug, Parser)]
pub struct NewFileCommand {
    /// File path
    #[arg(short, long)]
    file_path: PathBuf,

    /// File brief
    #[arg(short, long)]
    brief: Option<String>,
}

const CMAKE_FILE: &str = include_str!("../../templates/.cmake");
const CPP_FILE: &str = include_str!("../../templates/.cpp");
const CSHARP_FILE: &str = include_str!("../../templates/.cs");
const HPP_FILE: &str = include_str!("../../templates/.hpp");
const PS1_FILE: &str = include_str!("../../templates/.ps1");
const PY_FILE: &str = include_str!("../../templates/.py");
const SH_FILE: &str = include_str!("../../templates/.sh");
const RS_FILE: &str = include_str!("../../templates/.rs");

lazy_static! {
    static ref TEMPLATES: HashMap<&'static str, &'static str> = {
        let mut m = HashMap::new();
        m.insert("cmake", CMAKE_FILE);
        m.insert("cpp", CPP_FILE);
        m.insert("cs", CSHARP_FILE);
        m.insert("hpp", HPP_FILE);
        m.insert("ps1", PS1_FILE);
        m.insert("py", PY_FILE);
        m.insert("sh", SH_FILE);
        m.insert("rs", RS_FILE);

        m
    };
}

impl CliCommand for NewFileCommand {
    fn run(self) -> anyhow::Result<ExitCode> {
        let new_file_path = self.file_path;

        let brief = self.brief.unwrap_or_else(|| "".into());

        let extension = new_file_path
            .extension()
            .and_then(|p| p.to_str())
            .unwrap_or("");

        let template = TEMPLATES.get(extension);

        if template.is_none() {
            // Create it as-is
            File::create(new_file_path)?;

            info!("File created successfully");

            Ok(ExitCode::SUCCESS)
        } else if let Some(template) = template {
            // Render the template
            let mut env = minijinja::Environment::new();
            env.add_template("template", template)?;

            let filename = new_file_path
                .file_name()
                .and_then(|f| f.to_str())
                .ok_or_else(|| anyhow!("Cannot get filename"))?;
            let git_user = git_username()?;
            let current_date = chrono::offset::Local::now().format("%Y-%m-%d").to_string();

            let template = env.get_template("template")?;
            let content = template.render(context! (filename => filename,
                author => git_user,
                date => current_date,
                brief => brief
            ))?;

            let mut file = File::create(new_file_path)?;
            file.write_all(content.as_bytes())?;

            info!("File created from template successfully");

            Ok(ExitCode::SUCCESS)
        } else {
            error!("Could not create file");
            Ok(ExitCode::FAILURE)
        }
    }
}
