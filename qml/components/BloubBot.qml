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
    // One number drives every state; nothing accumulates. `entered` is when the
    // current mood began, so each state animates from its own zero.
    property real clock: 0
    /** When the current mood started. */
    property real entered: 0
    /** The mood being blended out of, and when it started. */
    property string previousMood: "idle"
    property real previousEntered: 0
    /** QML hands over the new value only, so the old one is kept by hand. */
    property string lastMood: "idle"

    onMoodChanged: {
        previousMood = lastMood
        previousEntered = entered
        entered = clock
        lastMood = mood
        canvas.requestPaint()
    }

    Timer {
        // 30 Hz: the reference material was cut at ten frames a second, and a
        // face this small gains nothing from sixty.
        interval: 33
        running: bot.visible && bot.alive
        repeat: true
        onTriggered: {
            bot.clock += 0.033
            canvas.requestPaint()
        }
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
     * at any instant depends only on the clock.
     */
    property var blinks: []

    Component.onCompleted: {
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
        blinks = out
    }

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
            const ramp = maths.easeInOutCubic(maths.clamp(t / 0.35))
            const rot = -maths.tau * 1.25 * t * ramp
            const fade = maths.clamp(t / 0.8)
            base.gaze = { yaw: restGaze.yaw + Math.sin(t * 6.5) * 22, pitch: 12, roll: -13 }
            base.eyes = [{ w: 0.18, h: 0.38 }, { w: 0.18, h: 0.38 }]
            base.rings = ringSeeds.map((seed, i) => ({
                seed: seed,
                t: t,
                opacity: fade * maths.clamp((t - i * 0.13) / 0.3)
            }))
            base.blink = false
            void rot
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
            // A small ball bobbing gently, with no face at all.
            base.radius = 0.1585
            base.cy = 0.11 + Math.sin(t * (maths.tau / 0.9)) * 0.19
            base.eyeAlpha = 0
            base.blink = false
            break

        default:
            break
        }

        return base
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

    // ---- rendering -------------------------------------------------------
    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        renderStrategy: Canvas.Cooperative

        function blendPose() {
            const now = bot.clock
            const current = bot.pose(bot.mood, now - bot.entered)
            // A change of mood cross-fades over its own length rather than
            // cutting, which is what keeps the eyes from teleporting.
            const age = now - bot.entered
            const morph = 0.42
            if (age >= morph || bot.previousMood === bot.mood) {
                return current
            }
            const k = maths.easeOutQuint(maths.clamp(age / morph))
            const before = bot.pose(bot.previousMood, now - bot.previousEntered)
            const mix = (a, b) => a + (b - a) * k
            return {
                radius: mix(before.radius, current.radius),
                cx: mix(before.cx, current.cx),
                cy: mix(before.cy, current.cy),
                gaze: {
                    yaw: mix(before.gaze.yaw, current.gaze.yaw),
                    pitch: mix(before.gaze.pitch, current.gaze.pitch),
                    roll: mix(before.gaze.roll, current.gaze.roll)
                },
                split: mix(before.split, current.split),
                eyes: [{ w: mix(before.eyes[0].w, current.eyes[0].w),
                         h: mix(before.eyes[0].h, current.eyes[0].h) },
                       { w: mix(before.eyes[1].w, current.eyes[1].w),
                         h: mix(before.eyes[1].h, current.eyes[1].h) }],
                eyeAlpha: mix(before.eyeAlpha, current.eyeAlpha),
                dots: current.dots,
                rings: current.rings,
                notif: current.notif,
                dotsBehind: current.dotsBehind,
                blink: current.blink && before.blink
            }
        }

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
            const p = blendPose()

            const life = bot.alive
                ? {
                      dYaw: (maths.loopNoise(bot.clock, 11.3, 0.4) * 5.5
                             + maths.loopNoise(bot.clock, 3.7, 2.1) * 1.6),
                      dPitch: (maths.loopNoise(bot.clock, 9.1, 1.3) * 4.2
                               + maths.loopNoise(bot.clock, 4.3, 0.7) * 1.3),
                      dRoll: maths.loopNoise(bot.clock, 13.7, 3.2) * 2.2,
                      lid: p.blink ? bot.blinkLid(bot.clock) : 1
                  }
                : { dYaw: 0, dPitch: 0, dRoll: 0, lid: 1 }

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

            // The body.
            ctx.fillStyle = bot.bodyColor
            ctx.beginPath()
            ctx.arc(p.cx, p.cy, Math.max(0.001, p.radius), 0, maths.tau)
            ctx.fill()

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
                const lid = 0.06 + 0.94 * maths.clamp(life.lid)
                ctx.globalCompositeOperation = "destination-out"
                for (let i = 0; i < 2; i++) {
                    const eye = poses[i]
                    if (eye.depth <= 0) {
                        continue
                    }
                    const shape = p.eyes[i]
                    ctx.save()
                    ctx.transform(eye.a, eye.b * lid, eye.c, eye.d * lid,
                                  p.cx + eye.x, p.cy + eye.y)
                    ctx.globalAlpha = p.eyeAlpha
                    const w = shape.w * p.radius
                    const h = shape.h * p.radius
                    const r = Math.min(w, h) / 2
                    // A capsule: a rounded rectangle whose radius is half its
                    // short axis, which is the shape the reference measures.
                    ctx.beginPath()
                    ctx.moveTo(-w / 2 + r, -h / 2)
                    ctx.lineTo(w / 2 - r, -h / 2)
                    ctx.arcTo(w / 2, -h / 2, w / 2, -h / 2 + r, r)
                    ctx.lineTo(w / 2, h / 2 - r)
                    ctx.arcTo(w / 2, h / 2, w / 2 - r, h / 2, r)
                    ctx.lineTo(-w / 2 + r, h / 2)
                    ctx.arcTo(-w / 2, h / 2, -w / 2, h / 2 - r, r)
                    ctx.lineTo(-w / 2, -h / 2 + r)
                    ctx.arcTo(-w / 2, -h / 2, -w / 2 + r, -h / 2, r)
                    ctx.closePath()
                    ctx.fill()
                    ctx.restore()
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
