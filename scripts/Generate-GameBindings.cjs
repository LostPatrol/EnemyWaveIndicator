// Generate the native binding profile from one reviewed JSON source; never discover or approve a new game automatically.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..'); // Repository containing the reviewed profile.
const profilePath = path.join(root, 'mods/EnemyWaveNativeProbe/GameBindingProfile.json'); // Single maintained binding source.
const headerPath = path.join(root, 'mods/EnemyWaveNativeProbe/GameBindingProfile.generated.h'); // Deterministic compiler input.

// Validate indices and operand masks before turning reviewed data into C++ constants.
function loadProfile(file = profilePath) {
    const raw = fs.readFileSync(file, 'utf8');
    const profile = JSON.parse(raw);
    assert.equal(profile.schema, 1);
    assert(profile.patterns.length > 0 && profile.patterns.length <= 16);
    assert(profile.edges.length <= 16);
    const names = new Map();
    for (const [i, pattern] of profile.patterns.entries()) {
        assert(/^[a-z][a-zA-Z0-9]*$/.test(pattern.name) && !names.has(pattern.name), 'Unique C++ site name required');
        names.set(pattern.name, i);
        assert(Number.isInteger(pattern.size) && pattern.size > 0 && pattern.size <= 0xffffffff);
        assert(/^[a-f0-9]{16}$/.test(pattern.normalizedHash));
        assert(/^(?:[a-f0-9]{2}|\?\?){1,64}$/.test(pattern.anchor));
        assert(pattern.anchor.replaceAll('??', '').length >= 16, 'Insufficient fixed anchor bytes');
        assert(pattern.anchor.length / 2 <= pattern.size && typeof pattern.functionStart === 'boolean');
        let end = 0;
        for (const [at, size] of pattern.masks) {
            assert(Number.isInteger(at) && at >= end && [1, 2, 4, 8].includes(size) && at + size <= pattern.size);
            end = at + size;
        }
        for (let at = 0; at < pattern.anchor.length / 2; ++at) {
            const masked = pattern.masks.some(([offset, width]) => at >= offset && at < offset + width);
            assert.equal(pattern.anchor.slice(at * 2, at * 2 + 2) === '??', masked, pattern.name + ' anchor mask');
        }
    }
    for (const edge of profile.edges) {
        assert(/^[a-z][a-zA-Z0-9]*$/.test(edge.name) && names.has(edge.owner) && names.has(edge.target));
        assert(Number.isInteger(edge.returnOffset) && edge.returnOffset >= 5 && edge.returnOffset <= profile.patterns[names.get(edge.owner)].size);
        assert(Number.isInteger(edge.targetOffset) && edge.targetOffset >= 0 && edge.targetOffset < profile.patterns[names.get(edge.target)].size);
    }
    assert.equal(new Set(profile.edges.map(edge => edge.name)).size, profile.edges.length);
    for (const window of profile.windows) {
        assert(names.has(window.owner) && /^[a-z][a-zA-Z0-9]*$/.test(window.name));
        assert(/^(?:[a-f0-9]{2}){1,64}$/.test(window.bytes));
        assert(Number.isInteger(window.offset) && window.offset >= 0 && window.offset + window.bytes.length / 2 <= profile.patterns[names.get(window.owner)].size);
    }
    assert(names.has(profile.threadPattern));
    assert.equal(profile.patterns[names.get(profile.threadPattern)].size, 22);
    for (const build of profile.knownBuilds) assert(/^\d+$/.test(build.steamBuild) && /^[A-F0-9]{64}$/.test(build.sha256));
    return {profile, names, sha256: crypto.createHash('sha256').update(JSON.stringify(profile)).digest('hex').toUpperCase()};
}

// Emit only deterministic data; runtime discovery and offline inspection consume this exact same table.
function render(loaded = loadProfile()) {
    const {profile, names, sha256} = loaded;
    const title = name => name[0].toUpperCase() + name.slice(1);
    const lines = ['// Generated from GameBindingProfile.json by scripts/Generate-GameBindings.cjs; edit the JSON, then regenerate.',
        '#pragma once', '#include "GameBindingResolver.h"', 'namespace ewi::bindings::profile {',
        '// Stable indices connect capture fields to discovered sites and actual call returns.',
        'enum class Site : uint8_t { ' + profile.patterns.map(item => title(item.name)).join(', ') + ' };',
        'enum class Call : uint8_t { ' + profile.edges.map(item => title(item.name)).join(', ') + ' };',
        '// Digest identifies the reviewed source data in diagnostics and build evidence.',
        'inline constexpr const char* Sha256 = "' + sha256 + '";'];
    for (const pattern of profile.patterns) lines.push('// Audited address operands for ' + pattern.name + '; all other bytes remain significant.',
        'inline constexpr std::array<MaskRange, ' + pattern.masks.length + '> Masks' + title(pattern.name) + '{{' +
        pattern.masks.map(([at, size]) => '{' + at + ',' + size + '}').join(',') + '}};');
    lines.push('// Full-span hashes and short search anchors jointly require unique instruction shapes.',
        'inline constexpr std::array<Pattern, ' + profile.patterns.length + '> Patterns{{');
    for (const pattern of profile.patterns) lines.push('    {"' + pattern.name + '", ' + pattern.size + ', 0x' + pattern.normalizedHash + 'ull, "' + pattern.anchor + '", Masks' + title(pattern.name) + '.data(), Masks' + title(pattern.name) + '.size(), ' + pattern.functionStart + '},');
    lines.push('}};', '// Call edges preserve exact source provenance after relocation.',
        'inline constexpr std::array<Edge, ' + profile.edges.length + '> Edges{{');
    for (const edge of profile.edges) lines.push('    {"' + edge.name + '", ' + names.get(edge.owner) + ', ' + edge.returnOffset + ', ' + names.get(edge.target) + ', ' + edge.targetOffset + '},');
    lines.push('}};', '// Independently audited queue operands remain mandatory ABI checks.',
        'inline constexpr std::array<Window, ' + profile.windows.length + '> Windows{{');
    for (const window of profile.windows) lines.push('    {"' + window.name + '", ' + names.get(window.owner) + ', ' + window.offset + ', "' + window.bytes + '"},');
    lines.push('}};', '// One profile is shared by production and the standalone native inspector.',
        'inline constexpr Profile Data{Patterns.data(), Patterns.size(), Edges.data(), Edges.size(), Windows.data(), Windows.size(), ' + names.get(profile.threadPattern) + '};',
        '} // namespace ewi::bindings::profile', '');
    return lines.join('\n');
}

// Refresh a reviewable candidate after an operator has established the new span boundaries and call/window offsets.
function refreshProfile(game, reviewedFile, decoderPath, output) {
    assert(output && path.resolve(output).toLowerCase() !== profilePath.toLowerCase(), 'Write candidates separately from the production profile');
    assert(decoderPath, 'Pass --decoder with the existing project iced-x86 module path; no dependency is installed automatically');
    const iced = require(path.resolve(decoderPath));
    const candidate = loadProfile(reviewedFile).profile;
    const bytes = fs.readFileSync(game), nt = bytes.readUInt32LE(0x3c), opt = nt + 24;
    assert.equal(bytes.readUInt32LE(nt), 0x4550); assert.equal(bytes.readUInt16LE(opt), 0x20b);
    const sections = Array.from({length: bytes.readUInt16LE(nt + 6)}, (_, i) => {
        const at = opt + bytes.readUInt16LE(nt + 20) + i * 40;
        return {rva: bytes.readUInt32LE(at + 12), size: bytes.readUInt32LE(at + 16), raw: bytes.readUInt32LE(at + 20)};
    });
    for (const pattern of candidate.patterns) {
        const start = pattern.baselineRva, end = start + pattern.size;
        const section = sections.find(item => start >= item.rva && end <= item.rva + item.size);
        assert(section && section.raw + end - section.rva <= bytes.length, pattern.name + ' file span');
        const span = Buffer.from(bytes.subarray(section.raw + start - section.rva, section.raw + end - section.rva));
        const decoder = new iced.Decoder(64, span, 0); decoder.ip = BigInt(start);
        const masks = [];
        while (decoder.canDecode) {
            const instruction = decoder.decode(), at = Number(instruction.ip) - start;
            assert.notEqual(instruction.code, iced.Code.INVALID, pattern.name + ' invalid instruction');
            const constants = decoder.getConstantOffsets(instruction);
            if (instruction.isIpRelMemoryOperand) masks.push([at + constants.displacementOffset, constants.displacementSize]);
            if ([iced.FlowControl.Call, iced.FlowControl.UnconditionalBranch, iced.FlowControl.ConditionalBranch].includes(instruction.flowControl)) {
                const target = Number(instruction.nearBranchTarget);
                if (target < start || target >= end) masks.push([at + constants.immediateOffset, constants.immediateSize]);
            }
            constants.free(); instruction.free();
        }
        decoder.free(); masks.sort((a, b) => a[0] - b[0]);
        const normalized = Buffer.from(span);
        for (const [at, size] of masks) normalized.fill(0, at, at + size);
        let hash = 14695981039346656037n;
        for (const byte of normalized) hash = BigInt.asUintN(64, (hash ^ BigInt(byte)) * 1099511628211n);
        pattern.masks = masks; pattern.normalizedHash = hash.toString(16).padStart(16, '0');
        pattern.anchor = [...span.subarray(0, Math.min(span.length, 64))].map((byte, i) =>
            masks.some(([at, size]) => i >= at && i < at + size) ? '??' : byte.toString(16).padStart(2, '0')).join('');
    }
    candidate._comment = 'Candidate from reviewed span locations. Audit instruction/ABI changes and call/window offsets before adopting; no build is approved here.';
    candidate.knownBuilds = [];
    fs.writeFileSync(output, JSON.stringify(candidate, null, 2) + '\n');
    loadProfile(output);
    return candidate;
}

if (require.main === module) {
    const argument = name => { const at = process.argv.indexOf(name); return at >= 0 ? process.argv[at + 1] : undefined; };
    if (argument('--refresh')) {
        refreshProfile(argument('--refresh'), argument('--profile') || profilePath, argument('--decoder'), argument('--output'));
        console.log('Candidate profile written separately; manual review and resolver tests are still required.');
        process.exit(0);
    }
    const expected = render(loadProfile(argument('--profile') || profilePath));
    const check = process.argv.includes('--check');
    const outputAt = process.argv.indexOf('--output');
    const output = outputAt >= 0 ? path.resolve(process.argv[outputAt + 1]) : headerPath;
    if (check) assert.equal(fs.readFileSync(output, 'utf8').replaceAll('\r\n', '\n'), expected, 'Binding header is stale; run node scripts/Generate-GameBindings.cjs');
    else fs.writeFileSync(output, expected);
    console.log('PASS: reviewed binding profile ' + (check ? 'matches' : 'generated') + ' native header.');
}
module.exports = {loadProfile, render, refreshProfile, profilePath, headerPath};
