// Read-only validation of production bindings, call-return edges and observed ABI against an on-disk game EXE.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..'); // Source tree whose bindings are checked.
const expectedGameSha = '112B29CAA86D643210571FBBF841C384E93C06AECBE6D0F12C1F71BA91F7A06D'; // Steam build 25433570.

// Parse PE32+ sections and named Windows imports without loading or executing the image.
function image(file) {
    const bytes = fs.readFileSync(file), nt = bytes.readUInt32LE(0x3c), opt = nt + 24;
    assert.equal(bytes.readUInt32LE(nt), 0x4550);
    assert.equal(bytes.readUInt16LE(opt), 0x20b);
    const sections = Array.from({length: bytes.readUInt16LE(nt + 6)}, (_, i) => {
        const at = opt + bytes.readUInt16LE(nt + 20) + i * 40;
        return {rva: bytes.readUInt32LE(at + 12), size: bytes.readUInt32LE(at + 16), offset: bytes.readUInt32LE(at + 20)};
    });
    // Require each checked byte range to have file backing in a single section.
    function offset(rva, size = 1) {
        const section = sections.find(s => rva >= s.rva && rva + size <= s.rva + s.size);
        assert(section, `Unmapped RVA 0x${rva.toString(16)}`);
        return section.offset + rva - section.rva;
    }
    const read = (rva, size) => bytes.subarray(offset(rva, size), offset(rva, size) + size);
    const string = at => bytes.toString('ascii', at, bytes.indexOf(0, at));
    const imports = new Map();
    for (let at = offset(bytes.readUInt32LE(opt + 120)); bytes.readUInt32LE(at + 12); at += 20) {
        const iat = bytes.readUInt32LE(at + 16), names = offset(bytes.readUInt32LE(at));
        for (let i = 0; bytes.readBigUInt64LE(names + i * 8); i++) {
            const thunk = bytes.readBigUInt64LE(names + i * 8);
            if (!(thunk >> 63n)) imports.set(iat + i * 8, string(offset(Number(thunk)) + 2));
        }
    }
    return {read, imports, sha256: crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase()};
}

// Validate source-derived entries and independently audited layout windows against the actual file.
function verify(file) {
    const game = image(file);
    assert.equal(game.sha256, expectedGameSha, 'Unrecognized game binary; re-audit before packaging');
    const source = fs.readFileSync(path.join(root, 'mods/EnemyWaveNativeProbe/AutomaticPresentation.cpp'), 'utf8');
    const identity = fs.readFileSync(path.join(root, 'mods/EnemyWaveNativeProbe/EngineThreadIdentity.h'), 'utf8');
    const entries = [...source.matchAll(/binding\.(\w+) = target\(base, (0x[0-9a-f]+), "([0-9a-f]+)"\)/g)]
        .filter(m => m[1] !== 'normalCenter').map(m => ({name: m[1], rva: Number(m[2]), signature: m[3]}));
    assert.equal(entries.length, 10);
    for (const entry of entries) assert.equal(game.read(entry.rva, entry.signature.length / 2).toString('hex'), entry.signature, entry.name);
    const targets = Object.fromEntries(entries.map(e => [e.name, e.rva]));
    const init = Number(identity.match(/InitCodeRva = (0x[0-9a-f]+)/)[1]);
    const thread = Number(identity.match(/ThreadIdRva = (0x[0-9a-f]+)/)[1]);
    const signature = [...identity.match(/Signature\[\] = \{([^}]+)\}/)[1].matchAll(/0x([0-9a-f]{2})/g)].map(m => m[1]).join('');
    const stores = game.read(init, 22);
    assert.equal(stores.toString('hex'), signature);
    assert.equal(game.imports.get(init + 6 + stores.readInt32LE(2)), 'GetCurrentThreadId');
    assert.equal(init + 22 + stores.readInt32LE(18), thread);
    assert.equal(init + 16 + stores.readInt32LE(12), thread + 16);
    const chain = [...source.match(/binding.sourceChain = \{([^}]+)\}/)[1].matchAll(/base \+ (0x[0-9a-f]+)/g)].map(m => Number(m[1]));
    assert.equal(chain.length, 6);
    const edges = chain.map((ret, i) => ({name: `chain${i}`, ret, target: [targets.enqueue, targets.center, 0x19da990, targets.pool, targets.normal, 0x16aa5a0][i]}));
    for (const [field, target] of [['normalReturn', targets.normal], ['actorReturn', targets.actor], ['shrinkReturn', targets.shrink]]) {
        edges.push({name: field, ret: Number(source.match(new RegExp(`binding\\.${field} = base \\+ (0x[0-9a-f]+)`))[1]), target});
    }
    for (const match of source.matchAll(/binding.enqueueReturns\[\d\] = base \+ (0x[0-9a-f]+)/g)) edges.push({name: 'enqueue', ret: Number(match[1]), target: targets.enqueue});
    for (const edge of edges) {
        const bytes = game.read(edge.ret - 5, 5);
        assert.equal(bytes[0], 0xe8, edge.name);
        assert.equal(edge.ret + bytes.readInt32LE(1), edge.target, edge.name);
    }
    assert.equal(edges.length, 11); // Six chain edges plus three dedicated and two alternate enqueue checks.
    // Independent disassembly windows prove component WorldPrivate, entry stride, descriptor and transform offsets.
    const layout = [
        [0x1656182, '488b81a8000000'], // Enqueue context -> WorldPrivate (+0xA8).
        [0x165fdb3, '49c1e507'],       // Queue entry index * 128.
        [0x165fde9, '498b4d28'],       // Descriptor at entry +0x28.
        [0x1660296, '4d8d4540'],       // Actor transform at entry +0x40.
        [0x16600c0, '410f105550'],     // Translation at entry +0x50.
        [0x166052e, '4c8db7c0010000']  // Manager queue array at +0x1C0.
    ];
    for (const [rva, bytes] of layout) assert.equal(game.read(rva, bytes.length / 2).toString('hex'), bytes, `layout 0x${rva.toString(16)}`);
    return {gamePath: path.resolve(file), gameSha256: game.sha256, gameSteamBuild: '25433570', entries, edges,
        engineIdentity: {init, thread}, layoutWindows: layout.length, staticChecksPassed: true, inGameVerified: false};
}

if (require.main === module) {
    const result = verify(process.argv[2]);
    if (process.argv[3]) fs.writeFileSync(process.argv[3], JSON.stringify(result, null, 2));
    console.log(`PASS: Steam ${result.gameSteamBuild}; 11 entry/thread signatures, 11 call-return checks, 6 ABI layout windows. Static evidence only.`);
}
module.exports = {verify};
