/** Thin host calls into the C/WebAssembly camera controller. */
const labSurfaceCamera = Object.freeze({
    fromAngles: angles => labDOM.call('lab_surface_camera', 0, angles, null, null, null, 0),
    rows: quaternion => labDOM.call('lab_surface_camera', 1, quaternion, null, null, null, 0),
    angles: (quaternion, previous) => labDOM.call('lab_surface_camera', 2, quaternion, previous, null, null, 0),
    drag: (quaternion, from, to, box, roll) => labDOM.call('lab_surface_camera', 3, quaternion, from, to, box, Number(roll)),
    multiply: (left, right) => labDOM.call('lab_surface_camera', 4, left, right, null, null, 0)
});
