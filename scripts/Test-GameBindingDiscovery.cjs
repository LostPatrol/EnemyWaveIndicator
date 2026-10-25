// Exercise the actual production profile on a real EXE and non-executable relocated-code fixtures.
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const {verify} = require('./Test-GameCompatibility.cjs');
const {loadProfile} = require('./Generate-GameBindings.cjs');
const root = path.resolve(__dirname, '..'); // All generated fixtures remain under agent/codex.
const codeRva = 0x1000, dataRva = 0x21000, metaRva = 0x22000; // Separate fixture code, globals and metadata sections.
const codeRaw = 0x400, dataRaw = 0x10400, metaRaw = 0x11400; // File offsets corresponding to the fixture sections.

// Map a real PE's file-backed RVA for copying audited instruction spans as data.
function diskImage(file) {
    const bytes = fs.readFileSync(file), nt = bytes.readUInt32LE(0x3c), opt = nt + 24;
    const sections = Array.from({length: bytes.readUInt16LE(nt + 6)}, (_, i) => {
        const at = opt + bytes.readUInt16LE(nt + 20) + i * 40;
        return {rva: bytes.readUInt32LE(at + 12), size: bytes.readUInt32LE(at + 16), raw: bytes.readUInt32LE(at + 20)};
    });
    return {read(rva, count) {
        const section = sections.find(item => rva >= item.rva && rva + count <= item.rva + item.size);
        assert(section, 'Missing real instruction span');
        return bytes.subarray(section.raw + rva - section.rva, section.raw + rva - section.rva + count);
    }};
}

// Construct a PE-shaped fixture; its instruction bytes are never loaded or executed.
function fixture(profile, source, real, shift) {
    const bytes = Buffer.alloc(metaRaw + 0x2000), opt = 0x98, table = opt + 0xf0;
    bytes.writeUInt16LE(0x5a4d, 0); bytes.writeUInt32LE(0x80, 0x3c); bytes.writeUInt32LE(0x4550, 0x80);
    bytes.writeUInt16LE(0x8664, 0x84); bytes.writeUInt16LE(3, 0x86); bytes.writeUInt16LE(0xf0, 0x94);
    bytes.writeUInt16LE(0x20b, opt); bytes.writeUInt32LE(0x25000, opt + 56); bytes.writeUInt32LE(0x400, opt + 60);
    bytes.writeUInt32LE(16, opt + 108);
    const sections = [[codeRva, 0x10000, codeRaw, 0x60000020], [dataRva, 0x1000, dataRaw, 0xc0000040], [metaRva, 0x2000, metaRaw, 0x40000040]];
    sections.forEach(([rva, size, raw, flags], i) => {
        const at = table + i * 40;
        bytes.writeUInt32LE(size, at + 8); bytes.writeUInt32LE(rva, at + 12);
        bytes.writeUInt32LE(size, at + 16); bytes.writeUInt32LE(raw, at + 20); bytes.writeUInt32LE(flags, at + 36);
    });
    const offset = rva => {
        const section = sections.find(([start, size]) => rva >= start && rva < start + size);
        assert(section); return section[2] + rva - section[0];
    };
    let next = codeRva + shift, count = 0;
    const positions = {};
    for (const pattern of profile.patterns) {
        next = (next + 0xff) & ~0xff;
        positions[pattern.name] = next;
        source.read(real.rvas[pattern.name], pattern.size).copy(bytes, offset(next));
        if (pattern.functionStart) {
            bytes.writeUInt32LE(next, metaRaw + count * 12);
            bytes.writeUInt32LE(next + pattern.size, metaRaw + count * 12 + 4); ++count;
        }
        next += pattern.size + 0x100;
    }
    bytes.writeUInt32LE(metaRva, opt + 136); bytes.writeUInt32LE(count * 12, opt + 140);
    for (const edge of profile.edges) {
        const end = positions[edge.owner] + edge.returnOffset, target = positions[edge.target] + edge.targetOffset;
        assert.equal(bytes[offset(end - 5)], 0xe8);
        bytes.writeInt32LE(target - end, offset(end - 4));
    }
    const iat = metaRva + 0x1000, lookup = metaRva + 0x1100, imports = metaRva + 0x1200, name = metaRva + 0x1300;
    bytes.writeUInt32LE(imports, opt + 120); bytes.writeUInt32LE(40, opt + 124);
    bytes.writeUInt32LE(lookup, offset(imports)); bytes.writeUInt32LE(name + 64, offset(imports + 12)); bytes.writeUInt32LE(iat, offset(imports + 16));
    bytes.writeBigUInt64LE(BigInt(name), offset(lookup));
    bytes.write('GetCurrentThreadId\0', offset(name + 2)); bytes.write('KERNEL32.dll\0', offset(name + 64));
    const thread = positions[profile.threadPattern];
    bytes.writeInt32LE(iat - thread - 6, offset(thread + 2));
    bytes.writeInt32LE(dataRva + 16 - thread - 16, offset(thread + 12));
    bytes.writeInt32LE(dataRva - thread - 22, offset(thread + 18));
    return {bytes, positions, offset, next, count, opt};
}

// Require both real-profile relocation success and rejection of semantic/ambiguous changes.
function run(game, output) {
    const profile = loadProfile().profile, real = verify(game);
    assert(real.staticChecksPassed, 'The supplied real game must match the reviewed instruction shapes');
    const source = diskImage(game), results = [];
    fs.mkdirSync(output, {recursive: true});
    for (const mode of ['relocated', 'relocatedAgain', 'wrongCall', 'wrongField', 'ambiguous']) {
        const item = fixture(profile, source, real, mode === 'relocatedAgain' ? 0x700 : 0x100);
        if (mode === 'wrongCall') {
            const edge = profile.edges.find(edge => edge.name === 'actor'), end = item.positions[edge.owner] + edge.returnOffset;
            item.bytes.writeInt32LE(item.positions.enqueue - end, item.offset(end - 4));
        }
        if (mode === 'wrongField') {
            const window = profile.windows.find(window => window.name === 'descriptor');
            item.bytes[item.offset(item.positions[window.owner] + window.offset + window.bytes.length / 2 - 1)] ^= 8;
        }
        if (mode === 'ambiguous') {
            const normal = profile.patterns.find(pattern => pattern.name === 'normal'), extra = (item.next + 0xff) & ~0xff;
            item.bytes.copy(item.bytes, item.offset(extra), item.offset(item.positions.normal), item.offset(item.positions.normal) + normal.size);
            const call = profile.edges.find(edge => edge.name === 'normal'), end = extra + call.returnOffset;
            item.bytes.writeInt32LE(item.positions[call.target] + call.targetOffset - end, item.offset(end - 4));
            item.bytes.writeUInt32LE(extra, metaRaw + item.count * 12); item.bytes.writeUInt32LE(extra + normal.size, metaRaw + item.count * 12 + 4);
            item.bytes.writeUInt32LE((item.count + 1) * 12, item.opt + 140);
        }
        const file = path.join(output, mode + '.pe'); fs.writeFileSync(file, item.bytes);
        const result = verify(file), expected = mode.startsWith('relocated');
        assert.equal(result.staticChecksPassed, expected, mode);
        assert.equal(result.releaseBaselineKnown, false, 'Fixture hash must stay explicitly unknown');
        if (expected) for (const [name, rva] of Object.entries(item.positions)) assert.equal(result.rvas[name], rva, mode + ':' + name);
        else if (mode === 'wrongCall') assert.equal(result.error, 'Call edge mismatch');
        else if (mode === 'ambiguous') assert.equal(result.error, 'Ambiguous pattern');
        results.push({mode, expected, result});
    }
    const report = {passed: true, realGame: real, fixtureResults: results, inGameVerified: false};
    fs.writeFileSync(path.join(output, 'verification.json'), JSON.stringify(report, null, 2) + '\n');
    console.log('PASS: real profile at two independent layouts; wrong call, changed queue field and duplicate target rejected. ' + output);
    return report;
}

if (require.main === module) {
    if (!process.argv[2]) throw new Error('Usage: node scripts/Test-GameBindingDiscovery.cjs <game.exe> [evidence-directory]');
    run(process.argv[2], process.argv[3] || path.join(root, 'agent/codex/binding-discovery-' + Date.now()));
}
module.exports = {run};
