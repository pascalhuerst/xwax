pub(crate) const ALPHA: f64 = 1.0/512.0;
pub(crate) const BETA: f64 = ALPHA/256.0;

#[derive(Debug)]
pub struct Pitch {
    dt: f64,
    x: f64,
    v: f64,
}

impl Pitch {
    pub fn new(dt: f64) -> Self {
        Self {
            dt,
            x: 0.0,
            v: 0.0,
        }
    }

    pub fn current(&self) -> f64 {
        self.v
    }

    pub fn dt_observation(&mut self, dx: f64) {
        let predicted_x = self.x + self.v * self.dt;
        let predicted_v = self.v;

        let residual_x = dx - predicted_x;

        self.x = predicted_x + residual_x * ALPHA;
        self.v = predicted_v + residual_x * BETA / self.dt;

        self.x -= dx; // relative to previous
    }

    pub fn reset(&mut self) {
        self.x = 0.0;
        self.v = 0.0;
    }
} 