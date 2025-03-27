use anyhow::Result;
use clap::Parser;
use neowax::audio::{Engine, Track};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use tracing_subscriber::{fmt, prelude::*};

#[derive(Parser, Debug)]
#[command(author, version, about = "Vinyl timecode controlled audio player")]
struct Args {
    /// ALSA input device for timecode (e.g. "plughw:2,0,0")
    #[arg(short, long)]
    input_device: String,

    /// ALSA output device for audio playback (e.g. "plughw:2,0,1")
    #[arg(short, long)]
    output_device: String,

    /// Sample rate in Hz
    #[arg(short, long, default_value_t = 48000)]
    sample_rate: u32,

    /// Audio file to play
    #[arg(short, long)]
    file: String,

    /// Timecode format (serato_cd, serato_2a, traktor_a, etc.)
    #[arg(short, long, default_value = "serato_cd")]
    timecode: String,

    /// Vinyl speed multiplier (1.0 = 33rpm, use for 45rpm records)
    #[arg(long, default_value_t = 1.0)]
    speed: f64,

    /// Phono-level input (lower signal threshold)
    #[arg(long)]
    phono: bool,
}

fn main() -> Result<()> {
    tracing_subscriber::registry()
        .with(fmt::layer().compact().with_file(true).with_line_number(true))
        .init();

    let args = Args::parse();

    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();
    ctrlc::set_handler(move || {
        r.store(false, Ordering::SeqCst);
    })?;

    tracing::info!("Loading track: {}", args.file);
    let track = Track::from_file(&args.file)?;

    let engine = Engine::new(args.input_device, args.output_device, args.sample_rate);

    tracing::info!("Starting engine with timecode: {}", args.timecode);
    let (handle, _track_tx) = engine.run(
        running.clone(),
        Some(track),
        &args.timecode,
        args.speed,
        args.phono,
    );

    handle.join().unwrap();
    tracing::info!("Neowax completed");
    Ok(())
}
