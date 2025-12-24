import { appStore } from '../core/Store.js';

export class SimulatorView {
    constructor() {
        this.container = document.getElementById('sim-view');
        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.robot = null; // Groupe for Mesh
        this.telemetryFeatures = {
            lidar: null
        };

        this.initThree();
        appStore.subscribe(this.onStateChange.bind(this));

        this.animate();

        // Handle Resize
        window.addEventListener('resize', () => {
            if (this.camera && this.renderer) {
                const width = this.container.clientWidth;
                const height = this.container.clientHeight;
                this.camera.aspect = width / height;
                this.camera.updateProjectionMatrix();
                this.renderer.setSize(width, height);
            }
        });
    }

    initThree() {
        // 1. Scene
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x222233); // Slightly lighter than black

        // 2. Camera
        const width = this.container.clientWidth || 300; // Fallback
        const height = this.container.clientHeight || 200;
        this.camera = new THREE.PerspectiveCamera(45, width / height, 0.1, 100);
        this.camera.position.set(3, 3, 3);
        this.camera.lookAt(0, 0, 0);

        // 3. Renderer
        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.setSize(width, height);
        this.renderer.shadowMap.enabled = true;
        this.container.appendChild(this.renderer.domElement);

        // 4. Controls
        if (typeof THREE.OrbitControls !== 'undefined') {
            this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
            this.controls.enableDamping = true;
        } else {
            console.warn("OrbitControls not loaded");
        }

        // 5. Lights
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
        this.scene.add(ambientLight);

        const dirLight = new THREE.DirectionalLight(0xffffff, 0.8);
        dirLight.position.set(5, 10, 5);
        dirLight.castShadow = true;
        this.scene.add(dirLight);

        // 6. Helpers
        const gridHelper = new THREE.GridHelper(10, 10, 0x444444, 0x222222);
        this.scene.add(gridHelper);

        const axesHelper = new THREE.AxesHelper(0.5);
        this.scene.add(axesHelper);
    }

    onStateChange(state) {
        // Build Robot if not exists and config is ready
        if (!this.robot && state.hardware && state.hardware.dimensions) {
            this.buildRobot(state.hardware);
        }

        // Update Position if robot exists
        if (this.robot && state.telemetry.axes) {
            this.updateRobotPose(state.telemetry.axes);
        }
    }

    buildRobot(config) {
        console.log("Building Robot 3D Model...", config);
        this.robot = new THREE.Group();

        // 1. Chassis (Box)
        const dim = config.dimensions || {}; // Access safely
        const dL = dim.length || 0.9;
        const dW = dim.width || 0.6;
        const dH = dim.height || 0.3;

        const geometry = new THREE.BoxGeometry(dL, dH, dW); // ThreeJS (Width, Height, Depth) -> Local (Length, Height, Width) if rotated? 
        // Wait, default Box is centered.
        // Let's create a visual group.
        // X axis is Forward (Red). Z axis is Side (Blue).
        // So Box(Length, Height, Width) = (dL, dH, dW).
        const material = new THREE.MeshStandardMaterial({ color: 0x00aaff, roughness: 0.5, metalness: 0.1 });
        const chassis = new THREE.Mesh(geometry, material);
        chassis.position.y = dH / 2;
        chassis.castShadow = true;
        this.robot.add(chassis);

        // 1b. Front Indicator (Cockpit/Head) - To show direction
        const headGeo = new THREE.BoxGeometry(dL * 0.2, dH * 0.8, dW * 0.8);
        const headMat = new THREE.MeshStandardMaterial({ color: 0x222222 });
        const head = new THREE.Mesh(headGeo, headMat);
        head.position.set(dL / 2 - dL * 0.1, dH, 0); // Front top
        this.robot.add(head);

        // 1c. Wheels (Visual only)
        const wheelGeo = new THREE.CylinderGeometry(dH * 0.4, dH * 0.4, dW * 0.1, 16);
        wheelGeo.rotateX(Math.PI / 2); // Make flat like a wheel
        const wheelMat = new THREE.MeshStandardMaterial({ color: 0x111111 });

        const w1 = new THREE.Mesh(wheelGeo, wheelMat); w1.position.set(dL / 3, dH * 0.4, dW / 2); this.robot.add(w1);
        const w2 = new THREE.Mesh(wheelGeo, wheelMat); w2.position.set(-dL / 3, dH * 0.4, dW / 2); this.robot.add(w2);
        const w3 = new THREE.Mesh(wheelGeo, wheelMat); w3.position.set(dL / 3, dH * 0.4, -dW / 2); this.robot.add(w3);
        const w4 = new THREE.Mesh(wheelGeo, wheelMat); w4.position.set(-dL / 3, dH * 0.4, -dW / 2); this.robot.add(w4);

        // 2. Sensors
        if (config.sensors) {
            config.sensors.forEach(sensor => {
                if (sensor.mount) {
                    const mount = sensor.mount;
                    // Simple representation: Small Red Box for sensors
                    const sGeo = new THREE.BoxGeometry(0.1, 0.1, 0.1);
                    const sMat = new THREE.MeshStandardMaterial({ color: 0xff4444 });
                    const sMesh = new THREE.Mesh(sGeo, sMat);
                    sMesh.position.set(mount.x, mount.y, mount.z); // Local offset
                    // Basic rotations if needed (simple for now)
                    this.robot.add(sMesh);
                }
            });
        }

        // Add to Scene
        this.scene.add(this.robot);
    }

    updateRobotPose(axes) {
        // axes: [x, y, theta, lift]
        // Map to 3D world
        // ThreeJS Y is Up. AMR Z is Up.
        // We need usually:
        // World X = AMR X
        // World Z = -AMR Y (Right hand rule Y up? No, ThreeJS Y is up.)
        // Standard WebGL: Y up, -Z forward?
        // Let's assume a simple ground plane XZ.
        // Robot X -> World X.
        // Robot Y -> World -Z (or Z).
        // Robot Theta -> Rotation around Y.

        const x = axes[0];
        const y = axes[1];
        const theta = axes[2];

        this.robot.position.set(x, 0, y); // Map AMR Y to World Z (Positive Z is "Back" or "Front" depending on camera)
        // Let's stick to: X is Red axis, Z is Blue axis. 
        // Rotation around Y (Green).
        this.robot.rotation.y = -theta; // CCW vs CW?
    }

    animate() {
        requestAnimationFrame(this.animate.bind(this));

        // Auto-Resize Check
        if (this.container && this.renderer && this.camera) {
            const width = this.container.clientWidth;
            const height = this.container.clientHeight;
            const canvas = this.renderer.domElement;

            // Check if resize needed (or if canvas is 0)
            if (canvas.width !== width || canvas.height !== height) {
                this.camera.aspect = width / height;
                this.camera.updateProjectionMatrix();
                this.renderer.setSize(width, height);
                console.log(`[SimView] Resized to ${width}x${height}`);
            }
        }

        if (this.controls) this.controls.update();
        if (this.renderer && this.scene && this.camera) {
            this.renderer.render(this.scene, this.camera);
        }
    }
}
