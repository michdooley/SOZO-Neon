// Three.js scene factory for the pattern editor.
//
// Builds the same 30 emissive torus meshes the live visualizer uses, but
// scoped to a caller-provided container element (so the editor sidebar can
// share the page with the preview). Exposes setLed/setAll and starts its
// own render loop. Mirrors index.html's geometry/material choices so the
// preview matches what you'll see in the real visualizer.

import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';

const SVG_W = 3747, SVG_H = 2736;
const BOARD_W = 52, BOARD_H = 38;
const SCALE = BOARD_W / SVG_W;
const TUBE_THICKNESS = 0.333;
const TUBE_HALF = TUBE_THICKNESS / 2;
const MAX_EMISSIVE = 3.0;

const CIRCLES = new Map();
const k = (cx, cy) => `${cx.toFixed(2)},${cy.toFixed(2)}`;
[
  [301.30, 2053.74, 157.72], [462.57, 1552.65, 133.89],
  [1636.14, 2377.45, 146.55], [1725.45, 1934.40, 134.36],
  [1859.80, 1558.54, 123.71], [687.81, 1147.27, 124.14],
  [963.67, 937.79, 118.23], [1273.21, 1041.17, 106.69],
  [1480.26, 1280.71, 106.69], [2512.46, 1509.32, 106.69],
  [2306.01, 1301.09, 106.69], [2044.07, 1322.70, 118.28],
  [1597.22, 940.16, 100.13], [2548.92, 1142.82, 100.13],
  [1762.04, 630.03, 86.50], [2635.42, 860.81, 86.50],
  [1970.71, 443.51, 80.05], [2832.87, 705.25, 80.05],
  [2228.26, 484.86, 73.70], [2364.13, 704.16, 73.70],
  [3149.95, 877.64, 73.70], [3035.94, 727.29, 73.70],
  [2437.82, 407.17, 68.24], [2600.86, 196.28, 68.24],
  [2828.14, 155.08, 68.24], [2993.40, 291.56, 68.24],
  [3184.79, 599.92, 68.24], [3266.31, 396.12, 68.24],
  [3439.98, 332.76, 68.24], [3613.66, 431.12, 68.24],
].forEach(([cx, cy, r]) => CIRCLES.set(k(cx, cy), { cx, cy, r }));

const LINE1_PATH = [
  [301.30, 2053.74], [462.57, 1552.65], [687.81, 1147.27],
  [963.67, 937.79], [1273.21, 1041.17], [1480.26, 1280.71],
  [1597.22, 940.16], [1762.04, 630.03], [1970.71, 443.51],
  [2228.26, 484.86], [2364.13, 704.16],
  [2437.82, 407.17], [2600.86, 196.28], [2828.14, 155.08],
  [2993.40, 291.56],
];
const LINE2_PATH = [
  [1636.14, 2377.45], [1725.45, 1934.40], [1859.80, 1558.54],
  [2044.07, 1322.70], [2306.01, 1301.09],
  [2512.46, 1509.32], [2548.92, 1142.82], [2635.42, 860.81],
  [2832.87, 705.25], [3035.94, 727.29],
  [3149.95, 877.64], [3184.79, 599.92], [3266.31, 396.12],
  [3439.98, 332.76], [3613.66, 431.12],
];

function svgToWorld(cx, cy) {
  return new THREE.Vector3(
    (cx - SVG_W / 2) * SCALE,
    -(cy - SVG_H / 2) * SCALE,
    0,
  );
}

export function initScene(container) {
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x000000);

  let w = container.clientWidth || 800;
  let h = container.clientHeight || 600;

  const camera = new THREE.PerspectiveCamera(40, w / h, 0.1, 500);
  camera.position.set(0, 0, 75);

  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setSize(w, h);
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.0;
  container.appendChild(renderer.domElement);

  const controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;

  const backboard = new THREE.Mesh(
    new THREE.PlaneGeometry(BOARD_W, BOARD_H),
    new THREE.MeshBasicMaterial({ color: 0x000000 }),
  );
  backboard.position.z = -0.4;
  scene.add(backboard);

  const frame = new THREE.LineSegments(
    new THREE.EdgesGeometry(new THREE.PlaneGeometry(BOARD_W, BOARD_H)),
    new THREE.LineBasicMaterial({ color: 0x222222 }),
  );
  frame.position.z = -0.39;
  scene.add(frame);

  scene.add(new THREE.AmbientLight(0xffffff, 0.18));
  const keyLight = new THREE.DirectionalLight(0xffffff, 0.25);
  keyLight.position.set(2, 3, 8);
  scene.add(keyLight);

  const toruses = [];
  const seen = new Set();
  function addFromPath(path) {
    for (const [cx, cy] of path) {
      const key = k(cx, cy);
      if (seen.has(key)) continue;
      const c = CIRCLES.get(key);
      if (!c) continue;
      const outerR = c.r * SCALE;
      const majorR = outerR - TUBE_HALF;
      const geo = new THREE.TorusGeometry(majorR, TUBE_HALF, 12, 64);
      const mat = new THREE.MeshStandardMaterial({
        color: 0x180404,
        emissive: 0xff0000,
        emissiveIntensity: 0.0,
        roughness: 0.45,
        metalness: 0.0,
      });
      const mesh = new THREE.Mesh(geo, mat);
      mesh.position.copy(svgToWorld(c.cx, c.cy));
      scene.add(mesh);
      toruses.push(mesh);
      seen.add(key);
    }
  }
  addFromPath(LINE1_PATH);  // idx 0..14
  addFromPath(LINE2_PATH);  // idx 15..29

  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));
  composer.addPass(new UnrealBloomPass(new THREE.Vector2(w, h), 1.3, 0.7, 0.0));
  composer.addPass(new OutputPass());

  function resize() {
    w = container.clientWidth || 800;
    h = container.clientHeight || 600;
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    renderer.setSize(w, h);
    composer.setSize(w, h);
  }
  window.addEventListener('resize', resize);
  if ('ResizeObserver' in window) new ResizeObserver(resize).observe(container);

  function setLed(i, brightness) {
    if (i < 0 || i >= toruses.length) return;
    const v = Math.max(0, Math.min(255, brightness)) / 255;
    toruses[i].material.emissiveIntensity = MAX_EMISSIVE * v;
  }
  function setAll(b) { for (let i = 0; i < toruses.length; i++) setLed(i, b); }

  function loop() {
    requestAnimationFrame(loop);
    controls.update();
    composer.render();
  }
  loop();

  return { setLed, setAll, ledCount: toruses.length };
}
