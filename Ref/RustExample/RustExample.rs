//! User implementation of the `RustExample` F Prime component.
//!
//! This file is the *user* surface: the auto-generated `rust_example_base.rs`
//! handles the FFI plumbing, dispatch shims, and helper methods on
//! [`Context`].  Edit this file freely -- the build system will not
//! overwrite it.

use fprime_rust::CmdResponse;
use crate::rust_example_base::{Component, Context};

/// State for the `RustExample` component.  Held inside a `Box<dyn Component>`
/// allocated by the auto-generated FFI constructor.
pub struct Implementation {
    counter: u32,
}

impl Implementation {
    /// Construct the user state.  Called exactly once per component instance
    /// before the topology is wired.
    pub fn new() -> Self {
        Self { counter: 0 }
    }
}

impl Component for Implementation {
    fn Reset_cmd_handler(&mut self, ctx: &mut Context) {
        self.counter = 0;
        ctx.tlm_write_Counter(self.counter);
        ctx.log_ResetIssued();
        ctx.command_response(CmdResponse::Ok);
    }

    fn Bump_cmd_handler(&mut self, ctx: &mut Context, amount: u32) {
        let (threshold, _validity) = ctx.param_Threshold();
        let next = self.counter.saturating_add(amount).min(threshold);
        self.counter = next;
        ctx.tlm_write_Counter(self.counter);
        ctx.log_Bumped(self.counter);
        ctx.command_response(CmdResponse::Ok);
    }
}
