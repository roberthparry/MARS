/**
 * @file surface_camera.ts
 * @brief Typed camera capability adapter for native plot projection.
 *
 * Forwards angle, quaternion and gesture requests to WebAssembly. The browser stores
 * returned coordinates but does not duplicate camera mathematics.
 */

/** Native operation numbers defined by lab_surface_camera() in lab_surface_math.c. */
enum MarsSurfaceCameraOperation {
    FROM_ANGLES = 0,
    ROTATION_ROWS = 1,
    TO_ANGLES = 2,
    DRAG = 3,
    MULTIPLY = 4
}

/** Plain browser-to-native boundary used by the camera adapter. */
interface MarsSurfaceCameraNative {
    /** @brief Invoke native code; each camera method identifies its concrete result type. */
    call(name: string, ...args: unknown[]): unknown;
}

/** Typed host adapter; all camera arithmetic remains in C/WebAssembly. */
class MarsSurfaceCamera {
    /** @brief Retain the native projection capability; camera mathematics remains native-owned. */
    constructor(private readonly native: MarsSurfaceCameraNative) {}

    /** @brief Obtain a native quaternion from camera angles without retaining mutable camera state. */
    fromAngles(angles: MarsSurfaceAngles): MarsSurfaceQuaternion {
        return this.native.call('lab_surface_camera', MarsSurfaceCameraOperation.FROM_ANGLES,
                                angles, null, null, null, 0) as MarsSurfaceQuaternion;
    }

    /** @brief Return the native rotation rows for a quaternion; callers own the returned array. */
    rows(quaternion: MarsSurfaceQuaternion): number[][] {
        return this.native.call('lab_surface_camera', MarsSurfaceCameraOperation.ROTATION_ROWS,
                                quaternion, null, null, null, 0) as number[][];
    }

    /** @brief Convert orientation to native camera angles, using previous angles to retain continuity. */
    angles(quaternion: MarsSurfaceQuaternion, previous: MarsSurfaceAngles): MarsSurfaceAngles {
        return this.native.call('lab_surface_camera', MarsSurfaceCameraOperation.TO_ANGLES,
                                quaternion, previous, null, null, 0) as MarsSurfaceAngles;
    }

    /**
     * @brief Obtain the native orientation for a drag within the supplied chart rectangle.
     * The input quaternion is the gesture's starting orientation; roll restricts the native
     * gesture to rotation about the viewing direction. No browser state is changed here.
     */
    drag(quaternion: MarsSurfaceQuaternion, from: MarsSurfacePoint2, to: MarsSurfacePoint2,
         box: DOMRect, roll = false): MarsSurfaceQuaternion {
        return this.native.call('lab_surface_camera', MarsSurfaceCameraOperation.DRAG,
                                quaternion, from, to, box, Number(roll)) as MarsSurfaceQuaternion;
    }

    /** @brief Compose quaternions in the supplied order through the native camera implementation. */
    multiply(left: MarsSurfaceQuaternion, right: MarsSurfaceQuaternion): MarsSurfaceQuaternion {
        return this.native.call('lab_surface_camera', MarsSurfaceCameraOperation.MULTIPLY,
                                left, right, null, null, 0) as MarsSurfaceQuaternion;
    }
}

/** @brief Shared page-wide camera adapter; orientation state belongs to each plot, not this instance. */
const labSurfaceCamera: MarsSurfaceCamera = new MarsSurfaceCamera(labDOM);
