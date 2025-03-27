use neowax::audio::{configure_audio_devices, realtime::prioritize_thread, AlsaSettings};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use tracing_subscriber::{fmt, prelude::*};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::registry()
        .with(fmt::layer().compact().with_file(true).with_line_number(true))
        .init();

    prioritize_thread();

    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();
    ctrlc::set_handler(move || {
        r.store(false, Ordering::SeqCst);
    })?;

    let settings = AlsaSettings {
        input_device: "plug:pipewire".to_string(),
        output_device: "plug:pipewire".to_string(),
        num_input_channels: 2,
        num_output_channels: 2,
        sample_rate: 48000,
        buffer_size: None,
        period_size: None,
    };

    let (input_pcm, output_pcm, _) = configure_audio_devices(&settings)?;

    let period = input_pcm.hw_params_current()?.get_period_size()? as usize;
    input_pcm.prepare()?;
    output_pcm.prepare()?;
    input_pcm.start()?;

    let mut buf = vec![0i16; period * 2];

    tracing::info!("Audio passthrough running, period_size={}", period);
    while running.load(Ordering::SeqCst) {
        let frames = input_pcm.io_i16()?.readi(&mut buf)? as usize;
        output_pcm.io_i16()?.writei(&buf[..frames * 2])?;
    }

    tracing::info!("Audio passthrough completed");
    Ok(())
}
