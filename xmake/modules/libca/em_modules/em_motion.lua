local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_motion",
        deps = {"em_base"},
        sources = {
            "inverted_pendulum.c",
            "kin_ackermann.c",
            "kin_diff.c",
            "kin_mecanum.c",
            "kin_omni.c",
            "model_trajectory.c",
            "polynomial.c",
            "pwm_ptz.c",
            "s_curve.c",
            "trapezoidal.c"
        }
    })
end
