#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PowerState {
    Awake,
    Asleep,
    Dozing,
    Unknown,
}

pub fn parse_power_state(text: &str) -> PowerState {
    let mut state = PowerState::Unknown;
    for line in text.lines() {
        let line = line.trim();
        if line == "mWakefulness=Awake" || line == "mWakefulness=1" || line == "mInteractive=true"
            || line == "Wakefulness: Awake" {
            return PowerState::Awake;
        }
        if let Some(value) = line.strip_prefix("mWakefulness=") {
            state = match value {
                "Asleep" | "0" => PowerState::Asleep,
                "Dozing" | "3" => PowerState::Dozing,
                _ => PowerState::Unknown,
            };
        }
    }
    state
}

impl PowerState {
    pub fn permits_panel_override(self) -> bool {
        matches!(self, Self::Asleep | Self::Dozing)
    }
}

// Read the current settings at the wake boundary; never cache the startup mode.
pub fn hint_only_mode(master_enabled: bool, aod_content_enabled: bool) -> bool {
    !master_enabled || !aod_content_enabled
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn hint_mode_follows_runtime_aod_changes() {
        assert!(!hint_only_mode(true, true));
        assert!(hint_only_mode(false, true));
        assert!(hint_only_mode(false, false));
        assert!(hint_only_mode(true, false));
        assert!(!hint_only_mode(true, true));
    }
    #[test]
    fn screen_on_blocks_all_panel_writes() {
        assert!(!parse_power_state("  mWakefulness=Awake\n").permits_panel_override());
        assert!(!parse_power_state("mWakefulness=Dozing\nmInteractive=true").permits_panel_override());
    }
    #[test]
    fn known_screen_off_states_retain_aod_and_sofod() {
        assert!(parse_power_state("mWakefulness=Asleep").permits_panel_override());
        assert!(parse_power_state("  mWakefulness=Dozing\n").permits_panel_override());
    }
    #[test]
    fn missing_denied_timed_out_queries_cannot_blank_screen() {
        for text in ["", "Permission Denial: can't dump PowerManager", "DUMP TIMEOUT", "mWakefulness=Dreaming", "mWakefulnessChanging=false"] {
            assert!(!parse_power_state(text).permits_panel_override(), "{text}");
        }
    }
    #[test]
    fn unrelated_text_does_not_grant_panel_ownership() {
        assert!(!parse_power_state("reason: mWakefulness=Asleep").permits_panel_override());
    }
    #[test]
    fn coloros_duplicate_numeric_wakefulness_preserves_doze() {
        assert!(parse_power_state("  mWakefulness=Dozing\n mWakefulnessChanging=false\nmWakefulness=3\n").permits_panel_override());
        assert!(!parse_power_state("mWakefulness=Dozing\nmWakefulness=1").permits_panel_override());
    }
}
