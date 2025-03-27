use neowax::timecode::{TimecodeDef, Timecoder};
use std::io::{self, Read};

const SAMPLE_RATE: u32 = 44100;
const CHANNELS: usize = 2;
const INTERVAL: usize = 4096; // Print output every INTERVAL samples
const BUFFER_SIZE: usize = 4096 * CHANNELS;

fn main() -> io::Result<()> {
    // Initialize timecoder with Serato CD timecode definition
    let def = TimecodeDef::serato_cd();
    let mut timecoder = Timecoder::new(def, 1.0, SAMPLE_RATE, false);

    let mut buffer = vec![0u8; BUFFER_SIZE * 2]; // *2 because i16 is 2 bytes
    let mut stdin = io::stdin().lock();
    let mut sample_count = 0;

    loop {
        // Read a chunk of samples
        match stdin.read_exact(&mut buffer) {
            Ok(()) => {
                let samples: Vec<i16> = buffer
                    .chunks_exact(2)
                    .map(|chunk| i16::from_le_bytes([chunk[0], chunk[1]]))
                    .collect();

                timecoder.submit(&samples);

                sample_count += BUFFER_SIZE / CHANNELS;
                if sample_count % INTERVAL == 0 {
                    let time = sample_count as f64 / SAMPLE_RATE as f64;
                    let pitch = timecoder.get_pitch();

                    print!("{:.3}\tPitch: {:.6}", time, pitch);

                    if let Some((pos, _)) = timecoder.get_position() {
                        print!("\tPosition: {}", pos);
                    }
                    print!("\tValid bits: {}", timecoder.get_valid_counter());
                    print!("\tBitstream: {:08x}", timecoder.get_bitstream());
                    print!("\tTimecode: {:08x}", timecoder.get_timecode());
                    print!("\tLUT lookup: {}", if timecoder.has_position() { "found" } else { "not found" });
                    println!();
                }
            }
            Err(e) if e.kind() == io::ErrorKind::UnexpectedEof => break,
            Err(e) => return Err(e),
        }
    }

    Ok(())
}
