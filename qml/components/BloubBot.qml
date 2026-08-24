// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The assistant's face: one round body with two capsule eyes painted on it,
 * which morphs between a handful of moods.
 *
 * It is a port of "bloub" by Jérémy Perret (MIT licensed,
 * github.com/jeremy-prt/bloub), reduced to the states an assistant in a pill
 * actually needs. The numbers are kept exactly as they are there because they
 * were measured rather than chosen - the eyes lean the way they do, and the
 * near eye is squashed the way it is, because that is what a face painted on a
 * sphere and projected flat does. Rounding them to friendlier values is what
 * makes it stop reading as a face.
 *
 * The eyes are punched out of the body rather than drawn on top of it, so they
 * clip themselves against the silhouette and the island's own background shows
 * through - which is what keeps the bot looking like part of the island and
 * not like a sticker on it.
 *
 * Everything here is a pure function of `clock`, so the whole thing can be
 * paused, resumed, or asked what it looked like at any moment.
 */
Item {
    id: bot

    /** idle | thinking | listening | working | alert | notify | done | asleep */
    property string mood: "idle"
    /** Diameter of the ball, in pixels. */
    property real size: 22
    property color bodyColor: Theme.accent
    /** Whether the resting drift and the blinks run. */
    property bool alive: true

    readonly property real ballRadius: size / 2
    /** The orbit rings reach 1.4 ball radii, so the canvas has to be wider. */
    readonly property real margin: 1.58

    implicitWidth: Math.round(size * margin)
    implicitHeight: implicitWidth

    // ---- clock -----------------------------------------------------------
    //
    // One number drives every state; every pose is a function of it. `entered`
    // is when the current mood began, so each state animates from its own zero.
    property real clock: 0
    /** When the current mood started. */
    property real entered: 0
    /** The mood being blended out of, and when it started. */
    property string previousMood: "idle"
    property real previousEntered: 0
    /** QML hands over the new value only, so the old one is kept by hand. */
    property string lastMood: "idle"
    /** True once a mood has actually been left, so there is something to blend from. */
    property bool hasPrevious: false
    /** When a forced blink was triggered; a change of shape hides behind one. */
    property real blinkAt: -10

    /**
     * The pose the last mood change interrupted, frozen.
     *
     * Only one mood of history is kept, so a change that lands while a blend is
     * still running would otherwise start its own blend from the *full* pose of
     * the mood being left rather than from the half-blended frame that is
     * actually on screen. The reference measures that as a 36px jump against
     * the 8px of ordinary movement, and it is exactly what the island does when
     * it goes thinking -> working -> answering in quick succession.
     *
     * Freezing only in that case matters as much: freezing on every change
     * would stop the outgoing mood's own animation dead for the whole blend.
     */
    property var frozen: null

    onMoodChanged: {
        const now = clock
        const blending = hasPrevious && (now - entered) < moodMorph(lastMood)
        frozen = blending ? composed(now, lastMood, entered, previousMood, previousEntered, frozen)
                          : null
        previousMood = lastMood
        previousEntered = entered
        entered = now
        lastMood = mood
        hasPrevious = true
        if (blinksIn(mood)) {
            blinkAt = now
        }
        canvas.requestPaint()
    }

    FrameAnimation {
        // Driven by the render loop rather than by a timer.
        //
        // A timer that adds a fixed amount per tick runs at the rate the timer
        // happens to fire, which is not the rate the screen refreshes at: under
        // any load the two drift apart, and the face then moves in a way that
        // looks like a fault in the animation rather than in the clock. Asking
        // the frame how long it actually took costs nothing and is right.
        running: bot.visible
        onTriggered: {
            // A frame that took absurdly long - the island was hidden, the
            // session was suspended - must not teleport the animation.
            bot.clock += Math.min(frameTime, 0.1)
            canvas.requestPaint()
        }
    }

    /**
     * How long a mood takes to blend in, in seconds. Measured per state in the
     * reference rather than shared, because a bunch of orbits arriving takes
     * visibly longer to read than a pair of eyes narrowing.
     */
    function moodMorph(name) {
        switch (name) {
        case "thinking":
            return 0.4
        case "listening":
            return 0.55
        case "working":
            return 0.6
        case "alert":
            return 0.45
        case "notify":
        case "done":
        case "asleep":
            return 0.5
        default:
            return 0.45
        }
    }

    /** Whether arriving at this mood is hidden behind a blink, as in the video. */
    function blinksIn(name) {
        return name === "thinking" || name === "listening" || name === "notify"
            || name === "done"
    }

    // ---- maths -----------------------------------------------------------
    QtObject {
        id: maths

        readonly property real tau: Math.PI * 2

        function clamp(v, lo, hi) {
            const low = lo === undefined ? 0 : lo
            const high = hi === undefined ? 1 : hi
            return v < low ? low : (v > high ? high : v)
        }

        function easeOutCubic(t) {
            return 1 - Math.pow(1 - t, 3)
        }

        function easeOutQuint(t) {
            return 1 - Math.pow(1 - t, 5)
        }

        function easeInOutCubic(t) {
            return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2
        }

        /** Periodic noise: loops seamlessly, which is what keeps drift alive. */
        function loopNoise(t, period, seed) {
            const p = (t / period) * tau
            return 0.55 * Math.sin(p + seed)
                 + 0.30 * Math.sin(2 * p + seed * 1.7 + 1.1)
                 + 0.15 * Math.sin(3 * p + seed * 2.3 + 2.4)
        }

        function deg(d) {
            return (d * Math.PI) / 180
        }

        /** Rotate two vectors of an orthonormal frame within their plane. */
        function spin(u, v, angle) {
            const c = Math.cos(angle)
            const s = Math.sin(angle)
            return [[u[0] * c + v[0] * s, u[1] * c + v[1] * s, u[2] * c + v[2] * s],
                    [v[0] * c - u[0] * s, v[1] * c - u[1] * s, v[2] * c - u[2] * s]]
        }

        /**
         * Where the two eyes sit on the sphere, and the tangent frame each one
         * is drawn in. Index 0 is the inner eye, 1 the outer.
         */
        function eyePoses(yaw, pitch, roll, scale, split) {
            let f = [0, 0, 1]
            let right = [1, 0, 0]
            let down = [0, 1, 0]

            let pair = spin(f, right, deg(yaw))
            f = pair[0]; right = pair[1]
            pair = spin(down, f, deg(pitch))
            down = pair[0]; f = pair[1]
            pair = spin(right, down, deg(roll))
            right = pair[0]; down = pair[1]

            const build = side => {
                const rotated = spin(f, right, deg(split * side))
                const ef = rotated[0]
                const er = rotated[1]
                return {
                    x: ef[0] * scale,
                    y: ef[1] * scale,
                    a: er[0], b: er[1],
                    c: down[0], d: down[1],
                    depth: ef[2]
                }
            }
            return [build(-1), build(1)]
        }
    }

    // ---- measured constants ---------------------------------------------
    readonly property real eyeSplit: 15.46
    readonly property real eyeW: 0.186
    readonly property real eyeH: 0.412
    readonly property var restGaze: ({ yaw: 28.49, pitch: 28.62, roll: -13 })

    /**
     * A pre-drawn blink schedule. Deterministic and stateless, so the picture
     * at any instant depends only on the clock - and drawn here rather than on
     * completion, so the very first frames already have one.
     */
    readonly property var blinks: (function () {
        let a = 0x5eed >>> 0
        const rng = () => {
            a = (a + 0x6d2b79f5) >>> 0
            let t = Math.imul(a ^ (a >>> 15), 1 | a)
            t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t
            return ((t ^ (t >>> 14)) >>> 0) / 4294967296
        }
        const out = []
        let t = 1.4
        while (t < 900) {
            out.push(t)
            t += 1.9 + rng() * 2.7
            // Now and then a double blink, which is what stops the rhythm
            // reading as a metronome.
            if (rng() < 0.18) {
                out.push(t)
                t += 0.24
            }
        }
        return out
    })()

    function blinkLid(t) {
        for (let i = 0; i < blinks.length; i++) {
            const start = blinks[i]
            if (t < start) {
                break
            }
            const k = (t - start) / 0.18
            if (k >= 0 && k <= 1) {
                // Shuts fast, opens a little slower.
                return k < 0.45 ? 1 - k / 0.45 : (k - 0.45) / 0.55
            }
        }
        return 1
    }

    // ---- poses -----------------------------------------------------------
    /**
     * Everything one frame of one mood needs: where the body is, where the
     * eyes look, and whatever decoration belongs to it.
     */
    function pose(name, t) {
        const base = {
            radius: 1,
            cx: 0,
            cy: 0,
            gaze: { yaw: restGaze.yaw, pitch: restGaze.pitch, roll: restGaze.roll },
            split: eyeSplit,
            eyes: [{ w: eyeW, h: eyeH }, { w: eyeW, h: eyeH }],
            eyeAlpha: 1,
            dots: [],
            rings: [],
            notif: null,
            dotsBehind: false,
            blink: true
        }

        switch (name) {
        case "thinking": {
            // The ball becomes the middle dot, so the morph stays continuous
            // instead of the body vanishing and three dots appearing.
            const pulse = index => {
                const p = (((t - index * 0.5) / 1.5) % 1 + 1) % 1
                return maths.clamp(p < 0.5 ? (0.5 - 0.5 * Math.cos(p * maths.tau)) * 2 : 0)
            }
            const emerge = 0.3 + 0.7 * maths.easeOutCubic(maths.clamp(t / 0.3))
            const dotX = [-0.557, -0.013, 0.532]
            base.radius = 0.165 * (1 + 0.25 * pulse(1))
            base.cx = dotX[1]
            base.eyeAlpha = 0
            base.dots = [0, 2].map(i => {
                const k = pulse(i)
                return { x: dotX[i] * emerge, y: 0, r: 0.165 * (1 + 0.25 * k), opacity: 0.55 + 0.45 * k }
            })
            break
        }

        case "listening":
            // Eyes wide open and looking slightly up: attention, not surprise.
            base.gaze = { yaw: 6.92, pitch: -21.96, roll: 11.6 }
            base.split = 18.43
            base.eyes = [{ w: 0.356, h: 0.875 }, { w: 0.356, h: 0.875 }]
            break

        case "working": {
            // The orbits start from rest rather than at full speed: the time
            // they are drawn at is eased in, so the bunch spins up instead of
            // appearing already turning.
            const ramp = maths.easeInOutCubic(maths.clamp(t / 0.35))
            const spun = t * ramp
            const fade = maths.clamp(t / 0.8)
            base.gaze = { yaw: restGaze.yaw + Math.sin(t * 6.5) * 22, pitch: 12, roll: -13 }
            base.eyes = [{ w: 0.18, h: 0.38 }, { w: 0.18, h: 0.38 }]
            base.rings = ringSeeds.map((seed, i) => ({
                seed: seed,
                t: spun,
                opacity: fade * maths.clamp((t - i * 0.13) / 0.3)
            }))
            base.blink = false
            break
        }

        case "alert": {
            // A quick lean and a hard stare. The buzz is what makes it read as
            // urgent rather than merely off-centre.
            const buzz = Math.sin(t * 2.5 * maths.tau)
            base.gaze = { yaw: -6 + buzz * 4, pitch: -12, roll: 8 + buzz * 3 }
            base.split = 17.2
            base.eyes = [{ w: 0.30, h: 0.62 }, { w: 0.30, h: 0.62 }]
            base.notif = { x: Math.cos(maths.deg(-42)) * 1.003,
                           y: Math.sin(maths.deg(-42)) * 1.003,
                           r: 0.15,
                           critical: true }
            break
        }

        case "notify": {
            // The pastille pops to 114 % and settles; the gaze goes the other
            // way, as if the bot had just noticed it out of the corner.
            const p = maths.clamp(t / 0.45)
            const pop = 1 + 0.14 * Math.sin(p * Math.PI) * (1 - p * 0.35)
            base.gaze = { yaw: -21.94, pitch: -5.82, roll: -12.2 }
            base.split = 18.89
            base.eyes = [{ w: 0.505, h: 0.498 }, { w: 0.505, h: 0.498 }]
            base.notif = { x: Math.cos(maths.deg(-42)) * 1.003,
                           y: Math.sin(maths.deg(-42)) * 1.003,
                           r: 0.15 * (p < 1 ? pop : 1),
                           critical: false }
            break
        }

        case "done": {
            // Collapse to a point, scatter, then reassemble; the eyes only
            // come back once the body has.
            const collapse = 1 - 0.834 * maths.easeOutQuint(maths.clamp(t / 0.7))
            const regrow = maths.easeOutQuint(maths.clamp((t - 1.2) / 0.7))
            base.radius = collapse + (1 - collapse) * regrow
            base.eyeAlpha = maths.clamp((t - 1.35) / 0.4)
            base.dots = particles(t)
            base.dotsBehind = true
            base.blink = false
            break
        }

        case "asleep":
            // A small ball with no face, bouncing. The period is measured -
            // 0.6s, not a round number - and so is the travel; slowing it down
            // to look calmer is what makes it read as a stutter instead.
            base.radius = 0.1585
            base.cy = 0.11 + Math.sin(t * (maths.tau / 0.6)) * 0.19
            base.eyeAlpha = 0
            base.blink = false
            break

        default:
            break
        }

        return base
    }

    /**
     * One eye's outline, as a closed polygon already in ball radii.
     *
     * The points are put through the tangent frame here rather than by the
     * canvas. Qt's Context2D.transform() composes its matrix in the canvas's
     * own space instead of on top of the transform already in force, which is
     * not what the HTML canvas this code was written against does: a pure
     * translation passed to it moves nothing at all. An eye placed that way
     * lands in the wrong place, at the wrong angle, and half of it ends up
     * outside the silhouette - which is exactly how the face was going wrong.
     *
     * Two multiplications by hand are both exact and shorter than working
     * around it, and the arcs become a polygon nobody can tell from one: at
     * the size this is drawn, a cap of fourteen segments is under a tenth of a
     * pixel from the true curve.
     */
    function eyeOutline(eye, shape, radius, lid, originX, originY) {
        const w = shape.w * radius
        const h = shape.h * radius
        const r = Math.min(w, h) / 2
        // What is left of the long axis once the two round caps are taken off.
        const straight = Math.max(0, h / 2 - r)
        const steps = 14
        const points = []

        const place = (x, y) => {
            points.push([originX + eye.a * x + eye.c * y,
                         // The blink squashes the eye vertically on screen,
                         // about its own centre - hence before the offset is
                         // added, and on the y output only.
                         originY + (eye.b * x + eye.d * y) * lid])
        }

        for (let i = 0; i <= steps; i++) {
            const angle = (i / steps) * Math.PI
            place(Math.cos(angle) * r, -straight - Math.sin(angle) * r)
        }
        for (let i = 0; i <= steps; i++) {
            const angle = (i / steps) * Math.PI
            place(-Math.cos(angle) * r, straight + Math.sin(angle) * r)
        }
        return points
    }

    /** Five particles, one born every 0.2 s, each living 0.55 s. */
    function particles(t) {
        const out = []
        let a = 0xbeef >>> 0
        const rng = () => {
            a = (a + 0x6d2b79f5) >>> 0
            let x = Math.imul(a ^ (a >>> 15), 1 | a)
            x = (x + Math.imul(x ^ (x >>> 7), 61 | x)) ^ x
            return ((x ^ (x >>> 14)) >>> 0) / 4294967296
        }
        for (let i = 0; i < 5; i++) {
            const birth = i * 0.2
            const angle = rng() * maths.tau
            const rho0 = 0.58 + rng() * 0.18
            const u = t - birth
            if (u < 0 || u > 0.62) {
                continue
            }
            const rho = rho0 * Math.pow(0.75, u * 10)
            out.push({
                x: Math.cos(angle + u * Math.PI * 100 / 180) * rho,
                y: Math.sin(angle + u * Math.PI * 100 / 180) * rho,
                r: 0.04 + 0.028 * maths.clamp(u / 0.55),
                opacity: maths.clamp(u / 0.06) * maths.clamp((0.62 - u) / 0.08)
            })
        }
        return out
    }

    /**
     * Six orbit rings, each a circle in 3D seen almost edge on. Their spread of
     * tilts and speeds is what makes the bunch read as depth rather than as a
     * flat scribble.
     */
    readonly property var ringSeeds: [
        { a: 1.32, k: 0.09, tilt: 0.18, speed: 3.2, phase: 0.9, sweep: 0.72, hue: 8,   width: 0.055 },
        { a: 1.36, k: 0.21, tilt: 0.71, speed: 3.5, phase: 2.4, sweep: 0.64, hue: 68,  width: 0.052 },
        { a: 1.30, k: 0.38, tilt: 1.22, speed: 3.1, phase: 4.1, sweep: 0.79, hue: 131, width: 0.058 },
        { a: 1.38, k: 0.14, tilt: 1.86, speed: 3.6, phase: 0.3, sweep: 0.68, hue: 194, width: 0.050 },
        { a: 1.33, k: 0.30, tilt: 2.35, speed: 3.3, phase: 5.2, sweep: 0.75, hue: 252, width: 0.056 },
        { a: 1.35, k: 0.44, tilt: 2.92, speed: 3.4, phase: 3.3, sweep: 0.61, hue: 310, width: 0.054 }
    ]

    /**
     * Two poses mixed.
     *
     * Geometry interpolates, decoration cross-fades in opacity instead: a dot
     * on its way out and a dot on its way in are not the same dot, and sliding
     * one into the other is a movement nothing in the reference makes. The
     * pastille belongs to one mood or the other and simply changes hands
     * halfway.
     */
    function mixPose(a, b, t) {
        const out = 1 - t
        const mix = (x, y) => x + (y - x) * t
        const faded = (list, k) => (list || []).map(item => {
            const copy = {}
            for (const key in item) {
                copy[key] = item[key]
            }
            copy.opacity = (item.opacity === undefined ? 1 : item.opacity) * k
            return copy
        })

        return {
            radius: mix(a.radius, b.radius),
            cx: mix(a.cx, b.cx),
            cy: mix(a.cy, b.cy),
            gaze: {
                yaw: mix(a.gaze.yaw, b.gaze.yaw),
                pitch: mix(a.gaze.pitch, b.gaze.pitch),
                roll: mix(a.gaze.roll, b.gaze.roll)
            },
            split: mix(a.split, b.split),
            eyes: [{ w: mix(a.eyes[0].w, b.eyes[0].w), h: mix(a.eyes[0].h, b.eyes[0].h) },
                   { w: mix(a.eyes[1].w, b.eyes[1].w), h: mix(a.eyes[1].h, b.eyes[1].h) }],
            eyeAlpha: mix(a.eyeAlpha, b.eyeAlpha),
            dots: faded(a.dots, out).concat(faded(b.dots, t)),
            rings: faded(a.rings, out).concat(faded(b.rings, t)),
            notif: t < 0.5 ? a.notif : b.notif,
            dotsBehind: t < 0.5 ? a.dotsBehind : b.dotsBehind,
            blink: a.blink && b.blink
        }
    }

    /**
     * The pose at `now`, whatever blend is in flight included. Taking every
     * part of the state as an argument is what lets a mood change freeze the
     * frame it interrupted: it can ask for the composite of the mood it is
     * about to leave, from inside the change itself.
     */
    function composed(now, cur, curAt, prev, prevAt, frozenPose) {
        const since = Math.max(0, now - curAt)
        const current = pose(cur, since)
        const morph = moodMorph(cur)
        if (since >= morph) {
            return current
        }
        const origin = frozenPose ? frozenPose
                                  : (hasPrevious && prev !== cur
                                     ? pose(prev, Math.max(0, now - prevAt))
                                     : null)
        if (!origin) {
            return current
        }
        return mixPose(origin, current, maths.easeOutQuint(maths.clamp(since / morph)))
    }

    // ---- rendering -------------------------------------------------------
    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        // Painted on the same thread that asked for it: the item is a few
        // dozen pixels across, and handing it to a worker thread only buys a
        // frame of lag between the clock and what is on screen.
        renderStrategy: Canvas.Immediate

        /** One ring, split into the half in front of the body and the half behind. */
        function ringPath(ctx, seed, time, front) {
            const steps = 34
            const spanStart = seed.phase + time * seed.speed
            const span = seed.sweep * maths.tau
            let drawing = false
            ctx.beginPath()
            for (let i = 0; i <= steps; i++) {
                const angle = spanStart + (i / steps) * span
                const x0 = seed.a * Math.cos(angle)
                const y0 = seed.a * seed.k * Math.sin(angle)
                const z = Math.sin(angle) * Math.sqrt(Math.max(0, 1 - seed.k * seed.k))
                const c = Math.cos(seed.tilt)
                const s = Math.sin(seed.tilt)
                const x = x0 * c - y0 * s
                const y = x0 * s + y0 * c
                const visible = front ? z >= 0 : z < 0
                if (!visible) {
                    drawing = false
                    continue
                }
                if (!drawing) {
                    ctx.moveTo(x, y)
                    drawing = true
                } else {
                    ctx.lineTo(x, y)
                }
            }
            ctx.stroke()
        }

        function drawRings(ctx, rings, front) {
            for (const ring of rings) {
                if (ring.opacity <= 0.01) {
                    continue
                }
                ctx.save()
                ctx.globalAlpha = ring.opacity * (front ? 1 : 0.55)
                ctx.lineWidth = ring.seed.width
                ctx.lineCap = "round"
                ctx.strokeStyle = Qt.hsla(((ring.seed.hue % 360) + 360) % 360 / 360, 0.55, 0.62, 1)
                ringPath(ctx, ring.seed, ring.t, front)
                ctx.restore()
            }
        }

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()

            const radius = bot.ballRadius
            const centreX = width / 2
            const centreY = height / 2
            const now = bot.clock
            const p = bot.composed(now, bot.mood, bot.entered,
                                   bot.previousMood, bot.previousEntered, bot.frozen)

            // At rest the reference is very nearly still - the centre is stable
            // to a few thousandths and the radius is constant - so the life is
            // in the gaze and the blinking. The drift and the breath below are
            // deliberately tiny: they exist so the picture is not frozen, not
            // so the bot floats. A visible bob on top is the thing the
            // reference warns against by name.
            const life = bot.alive
                ? {
                      dYaw: (maths.loopNoise(now, 11.3, 0.4) * 5.5
                             + maths.loopNoise(now, 3.7, 2.1) * 1.6),
                      dPitch: (maths.loopNoise(now, 9.1, 1.3) * 4.2
                               + maths.loopNoise(now, 4.3, 0.7) * 1.3),
                      dRoll: maths.loopNoise(now, 13.7, 3.2) * 2.2,
                      lid: p.blink ? bot.blinkLid(now) : 1,
                      driftX: maths.loopNoise(now, 7.9, 1.9) * 0.006,
                      driftY: maths.loopNoise(now, 5.3, 0.3) * 0.007,
                      breath: 1 + Math.sin((now / 3.4) * maths.tau) * 0.005
                  }
                : { dYaw: 0, dPitch: 0, dRoll: 0, lid: 1, driftX: 0, driftY: 0, breath: 1 }

            // A change of shape hides behind a blink, as every one of them does
            // in the video. It closes and opens over 0.2s and only ever takes
            // the eye further shut than the schedule already had it.
            const forced = maths.clamp((now - bot.blinkAt) / 0.2)
            const lidNow = Math.min(life.lid, forced < 1 ? Math.abs(forced * 2 - 1) : 1)

            const bodyX = p.cx + life.driftX
            const bodyY = p.cy + life.driftY

            ctx.save()
            ctx.translate(centreX, centreY)
            // Work in ball radii from here on: every constant above is in them.
            ctx.scale(radius, radius)

            // Rings behind the body first, so the body occludes them.
            if (p.rings && p.rings.length > 0) {
                drawRings(ctx, p.rings, false)
            }
            if (p.dots && p.dots.length > 0 && p.dotsBehind) {
                for (const dot of p.dots) {
                    ctx.globalAlpha = dot.opacity
                    ctx.fillStyle = bot.bodyColor
                    ctx.beginPath()
                    ctx.arc(dot.x, dot.y, dot.r, 0, maths.tau)
                    ctx.fill()
                }
                ctx.globalAlpha = 1
            }

            // The body. The breath is a half-percent on the height only - the
            // width is measured constant - so it is applied as a scale about
            // the body's own centre rather than baked into the radius.
            ctx.save()
            ctx.translate(bodyX, bodyY)
            ctx.scale(1, life.breath)
            ctx.fillStyle = bot.bodyColor
            ctx.beginPath()
            ctx.arc(0, 0, Math.max(0.001, p.radius), 0, maths.tau)
            ctx.fill()
            ctx.restore()

            // The eyes, punched straight out of it. Because they remove rather
            // than cover, they clip themselves against the silhouette and the
            // island shows through - no cropping code anywhere.
            if (p.eyeAlpha > 0.01) {
                const poses = maths.eyePoses(p.gaze.yaw + life.dYaw,
                                             p.gaze.pitch + life.dPitch,
                                             p.gaze.roll + life.dRoll,
                                             1,
                                             p.split)
                // A blink squashes the eye vertically on screen, around its own
                // centre; it is not the open eye scaled along its tilted axis.
                const lid = 0.06 + 0.94 * maths.clamp(lidNow)
                ctx.globalCompositeOperation = "destination-out"
                for (let i = 0; i < 2; i++) {
                    const eye = poses[i]
                    // An eye that has gone round the limb is not drawn at all;
                    // the cut-off is the reference's, not zero, because an eye
                    // exactly edge-on is a line and reads as a scratch.
                    if (eye.depth <= 0.02) {
                        continue
                    }
                    const outline = bot.eyeOutline(eye, p.eyes[i], p.radius, lid,
                                                   bodyX + eye.x, bodyY + eye.y)
                    ctx.globalAlpha = p.eyeAlpha
                    ctx.beginPath()
                    ctx.moveTo(outline[0][0], outline[0][1])
                    for (let k = 1; k < outline.length; k++) {
                        ctx.lineTo(outline[k][0], outline[k][1])
                    }
                    ctx.closePath()
                    ctx.fill()
                }
                ctx.globalCompositeOperation = "source-over"
                ctx.globalAlpha = 1
            }

            // The pastille, with the notch that keeps it visually detached.
            if (p.notif) {
                ctx.globalCompositeOperation = "destination-out"
                ctx.beginPath()
                ctx.arc(p.notif.x, p.notif.y, p.notif.r + 0.054, 0, maths.tau)
                ctx.fill()
                ctx.globalCompositeOperation = "source-over"
                ctx.fillStyle = p.notif.critical ? Theme.critical : "#2496e8"
                ctx.beginPath()
                ctx.arc(p.notif.x, p.notif.y, p.notif.r, 0, maths.tau)
                ctx.fill()
            }

            if (p.dots && p.dots.length > 0 && !p.dotsBehind) {
                ctx.fillStyle = bot.bodyColor
                for (const dot of p.dots) {
                    ctx.globalAlpha = dot.opacity
                    ctx.beginPath()
                    ctx.arc(dot.x, dot.y, dot.r, 0, maths.tau)
                    ctx.fill()
                }
                ctx.globalAlpha = 1
            }

            if (p.rings && p.rings.length > 0) {
                drawRings(ctx, p.rings, true)
            }

            ctx.restore()
        }
    }
}
