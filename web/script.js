const HERO_X_IDX = 0
const HERO_Y_IDX = 1
const HERO_RAD_IDX = 2
const CAMERA_X_IDX = 3
const CAMERA_Y_IDX = 4
const PIXELS_PER_METER_IDX = 5
const CLOSEST_OBS_X_IDX = 6
const CLOSEST_OBS_Y_IDX = 7
const IS_ORBITING_IDX = 8
const SPACE_KEY_DOWN_IDX = 9
const GAME_OVER_IDX = 10;
const ORBITING_X_IDX = 11;
const ORBITING_Y_IDX = 12;
const LEFT_BOUNDARY_X_IDX = 13;
const RIGHT_BOUNDARY_X_IDX = 14;
const SCORE_IDX = 15;
const SCREEN_WIDTH_IDX = 16;
const SCREEN_HEIGHT_IDX = 17;

let CANVAS = null
let CTX = null
let WASM = null
let MEMORY = {}

let startTimeMs = null
let lastFrameTimeMs = null
let isSpaceKeyDown = false

// This is for calculating frame rate
const FRAME_RATE_HISTORY_BUFFER_CAPACITY = 100;
const frameRateHistory = new Float32Array(FRAME_RATE_HISTORY_BUFFER_CAPACITY);
let frameRateHistorySize = 0;
let frameRateHistoryWriteIndex = 0;

// TODO: pointer events for phone too
document.addEventListener("keydown", keyInput)
document.addEventListener("keyup", keyInput)
document.addEventListener("pointerdown", keyInput)
document.addEventListener("pointerup", keyInput)

function keyInput(event) {
  if (event.repeat) return event.preventDefault() // Ignore repeat keystrokes

  if (event.type === "keydown" || event.type === "pointerdown") {
    isSpaceKeyDown = true
  } else if (
    event.type === "keyup"
    || event.type === "pointerup"
    || event.type === "pointercancel"
  ) {
    isSpaceKeyDown = false;
  }
}

function onAnimationFrame() {
  currentTimeMs = performance.now();
  elapsedTimeSinceStartMs = currentTimeMs - startTimeMs;
  // the amount of seconds since the last frame
  elapsedTimeSinceLastFrameSeconds = (currentTimeMs - lastFrameTimeMs) / 1000.0

  // finally, update last frame time
  lastFrameTimeMs = currentTimeMs;

  // Update our frame rate counter
  frameRateHistory[frameRateHistoryWriteIndex] = elapsedTimeSinceLastFrameSeconds
  frameRateHistorySize = Math.min(frameRateHistorySize + 1, FRAME_RATE_HISTORY_BUFFER_CAPACITY)
  frameRateHistoryWriteIndex = (frameRateHistoryWriteIndex + 1) % FRAME_RATE_HISTORY_BUFFER_CAPACITY
  let deltaSum = 0
  for (let i = 0; i < frameRateHistorySize; i++) {
    deltaSum += frameRateHistory[i]
  }
  let avgDelta = deltaSum / frameRateHistorySize
  let currentRollingFramerate = Math.floor(avgDelta === 0 ? 0 : 1 / avgDelta)
  const message = document.getElementById("frame-rate")
  console.log(currentRollingFramerate)
  message.textContent = `${currentRollingFramerate} fps`

  // Run the game loop
  WASM.loop(
    elapsedTimeSinceLastFrameSeconds,
    elapsedTimeSinceStartMs,
    isSpaceKeyDown,
  )

  const screenWidth = MEMORY.STATE_BUFFER[SCREEN_WIDTH_IDX] | 0
  const screenHeight = MEMORY.STATE_BUFFER[SCREEN_HEIGHT_IDX] | 0

  // Draw the background
  CTX.clearRect(0, 0, screenWidth, screenHeight)
  CTX.fillStyle = "#000000" // BLACK
  CTX.fillRect(0, 0, screenWidth, screenHeight)

  // Draw the left and right boundaries
  const leftBoundaryX = MEMORY.STATE_BUFFER[LEFT_BOUNDARY_X_IDX]
  const rightBoundaryX = MEMORY.STATE_BUFFER[RIGHT_BOUNDARY_X_IDX]

  CTX.beginPath()
  CTX.moveTo(leftBoundaryX, 0) // "top left"
  CTX.lineTo(leftBoundaryX, screenHeight) // "bottom left"
  CTX.strokeStyle = "#ffffff"
  CTX.stroke()

  CTX.beginPath()
  CTX.moveTo(rightBoundaryX, 0) // "top right"
  CTX.lineTo(rightBoundaryX, screenHeight) // "bottom right"
  CTX.strokeStyle = "#ffffff"
  CTX.stroke()

  // Draw the hero
  const heroX = MEMORY.STATE_BUFFER[HERO_X_IDX]
  const heroY = MEMORY.STATE_BUFFER[HERO_Y_IDX]
  const heroRad = MEMORY.STATE_BUFFER[HERO_RAD_IDX]

  CTX.beginPath()
  CTX.arc(heroX, heroY, heroRad, 0, 2 * Math.PI)
  CTX.fillStyle = "#ffffff"
  CTX.fill()

  // Draw orbit lines
  // This properly interprets the 32 bit information as a signed int
  const stateSpaceDown = MEMORY.STATE_BUFFER[SPACE_KEY_DOWN_IDX] | 0;
  const isOrbiting = MEMORY.STATE_BUFFER[IS_ORBITING_IDX] | 0;
  const closestObsX = MEMORY.STATE_BUFFER[CLOSEST_OBS_X_IDX];
  const closestObsY = MEMORY.STATE_BUFFER[CLOSEST_OBS_Y_IDX];
  if (stateSpaceDown === 1 && isOrbiting === 0) {
    CTX.beginPath()
    CTX.moveTo(heroX, heroY)
    CTX.lineTo(closestObsX, closestObsY)
    CTX.strokeStyle = "#ff0000" // RED
    CTX.stroke()
  }

  if (isOrbiting === 1) {
    const orbitX = MEMORY.STATE_BUFFER[ORBITING_X_IDX];
    const orbitY = MEMORY.STATE_BUFFER[ORBITING_Y_IDX];
    CTX.beginPath()
    CTX.moveTo(heroX, heroY)
    CTX.lineTo(orbitX, orbitY)
    CTX.strokeStyle = "#ff0000" // RED
    CTX.stroke()
  }


  // Draw the obstacles
  for (let i = 0; i < MEMORY.OBS_X_BUFFER.length; i++) {
    const obsX = MEMORY.OBS_X_BUFFER[i]
    const obsY = MEMORY.OBS_Y_BUFFER[i]
    const obsRad = MEMORY.OBS_RAD_BUFFER[i]

    // If there is no valid obstacle in the slot, break
    // since the buffer will have no more valid data
    if (obsRad <= 0) break

    CTX.beginPath()
    CTX.arc(obsX, obsY, obsRad, 0, 2 * Math.PI)
    CTX.fillStyle = "#ffffff"
    CTX.fill()
  }

  // Draw score
  const score = MEMORY.STATE_BUFFER[SCORE_IDX] | 0;
  CTX.font = "24px serif";
  CTX.fillText(score.toString(), 20, 35);

  const gameOver = MEMORY.STATE_BUFFER[GAME_OVER_IDX]
  if (!gameOver) {
    window.requestAnimationFrame(onAnimationFrame)
  } else {
    // On gameover, update the text
    // TODO: make this better UX
    const message = document.getElementById("message");
    message.textContent = "Game over! :(";
  }
}

async function startGame() {
  // Pass functions to C
  const importObject = {
    env: {
      sinf: (f) => Math.sin(f),
      cosf: (f) => Math.cos(f),
      atan2f: (f1, f2) => Math.atan2(f1, f2),
      sqrtf: (f) => Math.sqrt(f),
    }
  }

  try {
    const { instance } = await WebAssembly.instantiateStreaming(
      fetch("game.wasm"),
      importObject
    )

    WASM = instance.exports

    // We need to get the canvas width and height to pass to wasm
    CANVAS = document.getElementById("game")
    CTX = CANVAS.getContext('2d')

    const uint32Epoch = Math.floor(Date.now() / 1000) >>> 0

    WASM.init(
      uint32Epoch, // seed
      CANVAS.width, // game width pixels
      CANVAS.height, // game height pixels
    )

    // TODO: export buffer sizes from wasm 
    MEMORY.STATE_BUFFER = new Float32Array(
      WASM.memory.buffer,
      WASM.stateBufferPointer(),
      WASM.stateBufferSize(),
    );
    MEMORY.OBS_X_BUFFER = new Float32Array(
      WASM.memory.buffer,
      WASM.obsXBufferPointer(),
      WASM.obsBufferSize(),
    );
    MEMORY.OBS_Y_BUFFER = new Float32Array(
      WASM.memory.buffer,
      WASM.obsYBufferPointer(),
      WASM.obsBufferSize(),
    );
    MEMORY.OBS_RAD_BUFFER = new Float32Array(
      WASM.memory.buffer,
      WASM.obsRadBufferPointer(),
      WASM.obsBufferSize(),
    );

    startTimeMs = performance.now();
    lastFrameTimeMs = performance.now();
    requestAnimationFrame(onAnimationFrame)
  } catch (e) {
    console.error("Could not fetch wasm")
    console.log(e)
  }
}

startGame()
