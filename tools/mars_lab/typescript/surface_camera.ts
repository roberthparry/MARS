/**
 * @file surface_camera.ts
 * @brief Typed camera capability adapter for native plot projection.
 *
 * Forwards angle, quaternion and gesture requests to WebAssembly. The browser stores
 * returned coordinates but does not duplicate camera mathematics.
 */
/** Typed host adapter; all camera arithmetic remains in C/WebAssembly. */
class MarsSurfaceCamera {
    /** Retain the native projection capability; camera mathematics remains native-owned. */
    public constructor(private readonly native: Pick<LabDOMBridge, 'call'>) {}
    /** @brief Obtain a native quaternion from camera angles without retaining mutable camera state. */
    fromAngles(angles: MarsSurfaceAngles): MarsSurfaceQuaternion {
        return this.native.call<MarsSurfaceQuaternion>('lab_surface_camera', 0, angles, null, null, null, 0);
    }
    /** @brief Return the native rotation rows for a quaternion; callers own the returned array. */
    rows(quaternion: MarsSurfaceQuaternion): number[] {
        return this.native.call<number[]>('lab_surface_camera', 1, quaternion, null, null, null, 0);
    }
    /** @brief Convert orientation to native camera angles, using previous angles to retain continuity. */
    angles(quaternion: MarsSurfaceQuaternion, previous: MarsSurfaceAngles): MarsSurfaceAngles {
        return this.native.call<MarsSurfaceAngles>('lab_surface_camera', 2, quaternion, previous, null, null, 0);
    }
    /**
     * @brief Obtain the native orientation for a drag within the supplied chart rectangle.
     * The input quaternion is the gesture's starting orientation; roll restricts the native
     * gesture to rotation about the viewing direction. No browser state is changed here.
     */
    drag(quaternion: MarsSurfaceQuaternion, from: MarsSurfacePoint2, to: MarsSurfacePoint2,
         box: DOMRect, roll = false): MarsSurfaceQuaternion {
        return this.native.call<MarsSurfaceQuaternion>('lab_surface_camera', 3, quaternion, from, to, box, Number(roll));
    }
    /** @brief Compose quaternions in the supplied order through the native camera implementation. */
    multiply(left: MarsSurfaceQuaternion, right: MarsSurfaceQuaternion): MarsSurfaceQuaternion {
        return this.native.call<MarsSurfaceQuaternion>('lab_surface_camera', 4, left, right, null, null, 0);
    }
}
const labSurfaceCamera: MarsSurfaceCamera = new MarsSurfaceCamera(labDOM);
