//! Smoke tests for the curve-profile FFI: construct a profile, build its
//! sampled segment table, and read a couple of samples back.

use bmesh_sys::*;

/// A preset profile initialised for N segments exposes an N+1 sample table
/// whose first and last samples sit at the profile's endpoints.
#[test]
fn preset_profile_samples_read_back() {
    // LINE preset: a straight profile from (1, 0) to (0, 1).
    let profile = unsafe { bms_curveprofile_new_preset(0) };
    assert!(!profile.is_null());

    let segments = 4;
    unsafe { bms_curveprofile_init(profile, segments) };

    // Table holds `segments + 1` samples, indices 0..=segments.
    let mut first = (f32::NAN, f32::NAN);
    let mut last = (f32::NAN, f32::NAN);
    let ok_first = unsafe {
        bms_curveprofile_segment_xy(profile, 0, &mut first.0, &mut first.1)
    };
    let ok_last = unsafe {
        bms_curveprofile_segment_xy(profile, segments, &mut last.0, &mut last.1)
    };
    assert!(ok_first && ok_last);

    // Out-of-range index is rejected, leaving the outputs untouched.
    let mut oob = (7.0_f32, 7.0_f32);
    let ok_oob = unsafe {
        bms_curveprofile_segment_xy(profile, segments + 1, &mut oob.0, &mut oob.1)
    };
    assert!(!ok_oob);
    assert_eq!(oob, (7.0, 7.0));

    // The line profile runs from (1, 0) to (0, 1).
    assert!((first.0 - 1.0).abs() < 1e-4, "first.x = {}", first.0);
    assert!(first.1.abs() < 1e-4, "first.y = {}", first.1);
    assert!(last.0.abs() < 1e-4, "last.x = {}", last.0);
    assert!((last.1 - 1.0).abs() < 1e-4, "last.y = {}", last.1);

    unsafe { bms_curveprofile_free(profile) };
}

/// A profile built from explicit control points samples back cleanly.
#[test]
fn explicit_points_profile_samples_read_back() {
    // Three control points with AUTO handles (handle enum: AUTO = 1).
    let xy: [f32; 6] = [1.0, 0.0, 0.5, 0.5, 0.0, 1.0];
    let h1: [i32; 3] = [1, 1, 1];
    let h2: [i32; 3] = [1, 1, 1];
    let profile = unsafe {
        bms_curveprofile_new_from_points(xy.as_ptr(), h1.as_ptr(), h2.as_ptr(), 3)
    };
    assert!(!profile.is_null());

    let segments = 8;
    unsafe { bms_curveprofile_init(profile, segments) };

    let mut sample = (f32::NAN, f32::NAN);
    let ok = unsafe {
        bms_curveprofile_segment_xy(profile, 2, &mut sample.0, &mut sample.1)
    };
    assert!(ok);
    assert!(sample.0.is_finite() && sample.1.is_finite());

    unsafe { bms_curveprofile_free(profile) };
}
