use anyhow::Result;
use clap::Parser;
use neowax::audiofile::AudioFile;
use std::path::PathBuf;
use std::io::Write;

#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// Path to the audio file to play
    #[arg(short, long)]
    file: PathBuf,

    /// Buffer size in samples
    #[arg(short, long, default_value_t = 1024)]
    buffer_size: usize,

    /// Print debug information
    #[arg(short, long)]
    debug: bool,
}

fn main() -> Result<()> {
    // Initialize logging
    tracing_subscriber::fmt::init();

    // Parse command line arguments
    let args = Args::parse();

    // Open the audio file
    let mut audio_file = AudioFile::open(&args.file)?;

    // Print metadata if debug is enabled
    if args.debug {
        let metadata = audio_file.metadata();
        println!("Audio File Information:");
        println!("----------------------");
        println!("Title: {:?}", metadata.title);
        println!("Artist: {:?}", metadata.artist);
        println!("Album: {:?}", metadata.album);
        println!("Year: {:?}", metadata.year);
        println!("Sample Rate: {} Hz", metadata.sample_rate);
        println!("Channels: {}", metadata.channels);
        println!("Duration: {:?}", metadata.duration);
        println!("----------------------");
    }

    // Process audio samples
    let mut total_samples = 0;
    while let Ok(Some(samples)) = audio_file.read_samples(args.buffer_size) {
        let len = samples.len();
        total_samples += len;

        // Convert samples to bytes and write to stdout
        for sample in samples {
            // Convert f32 [-1.0, 1.0] to i16 [-32768, 32767]
            let sample_i16 = (sample * 32767.0) as i16;
            
            // Write sample as little-endian bytes
            std::io::stdout().write(&sample_i16.to_le_bytes())?;
        }

        if args.debug {
            eprintln!("Processed {} samples", len);
        }
    }

    if args.debug {
        eprintln!("Total samples processed: {}", total_samples);
    }

    Ok(())
}
