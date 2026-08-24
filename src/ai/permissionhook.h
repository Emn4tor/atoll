/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

/**
 * `atoll --permission-hook`: the gate the command-line client is started with.
 *
 * The client runs this once for every tool call it is about to make, hands it
 * the call on standard input, and does what the answer on standard output
 * says. All this process does is carry the question to the running island and
 * the verdict back, so the decision is made by the same broker, and shown in
 * the same words, as for every other way of reaching the assistant.
 *
 * It answers "deny" whenever it cannot get a real answer - no island, no bus,
 * nobody home - because the alternative is a tool call that runs because
 * something was broken.
 */
int runPermissionHook();
