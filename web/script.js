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

let CANVAS = null
let CTX = null
let WASM = null
let MEMORY = {}

let startTimeMs = null
let lastFrameTimeMs = null
let isSpaceKeyDown = false

// TODO: pointer events for phone too
document.addEventListener("keydown", keyInput)
document.addEventListener("keyup", keyInput)

function keyInput(event) {
  if (event.repeat) return event.preventDefault() // Ignore repeat keystrokes

  if (event.type === "keydown") {
    isSpaceKeyDown = true
  } else if (event.type === "keyup") {
    isSpaceKeyDown = false
  }
}

function onAnimationFrame() {
  currentTimeMs = performance.now();
  elapsedTimeSinceStartMs = currentTimeMs - startTimeMs;
  // the amount of seconds since the last frame
  elapsedTimeSinceLastFrameSeconds = (currentTimeMs - lastFrameTimeMs) / 1000.0

  // finally, update last frame time
  lastFrameTimeMs = currentTimeMs;

  // Run the game loop
  WASM.loop(
    elapsedTimeSinceLastFrameSeconds,
    elapsedTimeSinceStartMs,
    isSpaceKeyDown,
  )

  // Draw the background
  CTX.clearRect(0, 0, CANVAS.width, CANVAS.height)
  CTX.fillStyle = "#000000" // BLACK
  CTX.fillRect(0, 0, CANVAS.width, CANVAS.height)

  // Draw the left and right boundaries
  const screenWidthMeters = WASM.pixelsToMeters(CANVAS.width)
  const rightBoundXMeters = (screenWidthMeters / 2)
  const leftBoundXMeters = (screenWidthMeters / 2) * -1
  const leftBoundXPixels = WASM.metersXToPixels(leftBoundXMeters, CANVAS.width, CANVAS.height)
  const rightBoundXPixels = WASM.metersXToPixels(rightBoundXMeters, CANVAS.width, CANVAS.height)

  CTX.beginPath()
  CTX.moveTo(leftBoundXPixels, 0) // "top left"
  CTX.lineTo(leftBoundXPixels, CANVAS.height) // "bottom left"
  CTX.strokeStyle = "#ffffff"
  CTX.stroke()

  CTX.beginPath()
  CTX.moveTo(rightBoundXPixels, 0) // "top right"
  CTX.lineTo(rightBoundXPixels, CANVAS.height) // "bottom right"
  CTX.strokeStyle = "#ffffff"
  CTX.stroke()

  // Draw the hero
  const heroX = MEMORY.STATE_BUFFER[HERO_X_IDX]
  const heroY = MEMORY.STATE_BUFFER[HERO_Y_IDX]
  const heroRad = MEMORY.STATE_BUFFER[HERO_RAD_IDX]

  CTX.beginPath()
  CTX.arc(
    WASM.metersXToPixels(heroX, CANVAS.width, CANVAS.height),
    WASM.metersYToPixels(heroY, CANVAS.width, CANVAS.height),
    WASM.metersToPixels(heroRad),
    0,
    2 * Math.PI,
  )
  CTX.fillStyle = "#ffffff"
  CTX.fill()

  // Draw the obstacles
  for (let i = 0; i < 20; i++) {
    const obsX = MEMORY.OBS_X_BUFFER[i]
    const obsY = MEMORY.OBS_Y_BUFFER[i]
    const obsRad = MEMORY.OBS_RAD_BUFFER[i]

    // If there is no valid obstacle in the slot, break
    // since the buffer will have no more valid data
    if (obsRad <= 0) break

    CTX.beginPath()
    CTX.arc(
      WASM.metersXToPixels(obsX, CANVAS.width, CANVAS.height),
      WASM.metersYToPixels(obsY, CANVAS.width, CANVAS.height),
      WASM.metersToPixels(obsRad),
      0,
      2 * Math.PI,
    )
    CTX.fillStyle = "#ffffff"
    CTX.fill()
  }

  // Draw orbit lines
  // This properly interprets the 32 bit information as a signed int
  const stateSpaceDown = MEMORY.STATE_BUFFER[SPACE_KEY_DOWN_IDX] | 0;
  const isOrbiting = MEMORY.STATE_BUFFER[IS_ORBITING_IDX] | 0;
  const closestObsX = MEMORY.STATE_BUFFER[CLOSEST_OBS_X_IDX];
  const closestObsY = MEMORY.STATE_BUFFER[CLOSEST_OBS_Y_IDX];
  if (stateSpaceDown === 1 && isOrbiting === 0) {
    CTX.beginPath()
    CTX.moveTo(
      WASM.metersXToPixels(heroX, CANVAS.width, CANVAS.height),
      WASM.metersYToPixels(heroY, CANVAS.width, CANVAS.height),
    )
    CTX.lineTo(
      WASM.metersXToPixels(closestObsX, CANVAS.width, CANVAS.height),
      WASM.metersYToPixels(closestObsY, CANVAS.width, CANVAS.height),
    )
    CTX.strokeStyle = "#ff0000" // RED
    CTX.stroke()
  }

  if (isOrbiting === 1) {
    const orbitX = MEMORY.STATE_BUFFER[ORBITING_X_IDX];
    const orbitY = MEMORY.STATE_BUFFER[ORBITING_Y_IDX];
    CTX.beginPath()
    CTX.moveTo(
      WASM.metersXToPixels(heroX, CANVAS.width, CANVAS.height),
      WASM.metersYToPixels(heroY, CANVAS.width, CANVAS.height),
    )
    CTX.lineTo(
      WASM.metersXToPixels(orbitX, CANVAS.width, CANVAS.height),
      WASM.metersYToPixels(orbitY, CANVAS.width, CANVAS.height),
    )
    CTX.strokeStyle = "#ff0000" // RED
    CTX.stroke()
  }

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

    instance.exports.init(
      uint32Epoch, // seed
      CANVAS.width, // game width pixels
      CANVAS.height, // game height pixels
    )

    // TODO: export buffer sizes from wasm 
    MEMORY.STATE_BUFFER = new Float32Array(WASM.memory.buffer, WASM.STATE_BUFFER.value, 13);
    MEMORY.OBS_X_BUFFER = new Float32Array(WASM.memory.buffer, WASM.OBS_X_BUFFER.value, 20);
    MEMORY.OBS_Y_BUFFER = new Float32Array(WASM.memory.buffer, WASM.OBS_Y_BUFFER.value, 20);
    MEMORY.OBS_RAD_BUFFER = new Float32Array(WASM.memory.buffer, WASM.OBS_RAD_BUFFER.value, 20);

    startTimeMs = performance.now();
    lastFrameTimeMs = performance.now();
    requestAnimationFrame(onAnimationFrame)
  } catch (e) {
    console.error("Could not fetch wasm")
    console.log(e)
  }
}

startGame()
