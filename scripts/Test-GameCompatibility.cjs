// Inspect an on-disk game with the same structural resolver and reviewed profile used by the production DLL.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const cp = require('node:child_process');
const {loadProfile} = require('./Generate-GameBindings.cjs');
const root = path.resolve(__dirname, '..'); // Inspector builds and evidence stay within this repository.

// Build the content-addressed inspector if necessary, then retain diagnostics even for unsupported games.
function verify(file) {
    const profile = loadProfile();
    const executable = cp.execFileSync('pwsh.exe', ['-NoProfile', '-File', path.join(__dirname, 'Build-GameBindingInspector.ps1')],
        {cwd: root, encoding: 'utf8', windowsHide: true}).trim();
    const run = cp.spawnSync(executable, [path.resolve(file)], {encoding: 'utf8', windowsHide: true, maxBuffer: 2 * 1024 * 1024});
    if (run.error) throw run.error;
    let native;
    try { native = JSON.parse(run.stdout.trim()); }
    catch { throw new Error('Native binding inspector failed to emit JSON: ' + (run.stderr || run.stdout).slice(0, 1000)); }
    const sha256 = fs.existsSync(file) ? crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex').toUpperCase() : null;
    const known = profile.profile.knownBuilds.find(build => build.sha256 === sha256);
    return {...native, gamePath: path.resolve(file), gameSha256: sha256,
        gameSteamBuild: known?.steamBuild ?? null, releaseBaselineKnown: Boolean(known),
        staticChecksPassed: native.passed && run.status === 0, inGameVerified: false,
        note: known ? 'Known release baseline; static evidence only.' :
            'Unknown file hash. Structural results remain available; gameplay and release approval require review.'};
}

if (require.main === module) {
    const args = process.argv.slice(2).filter(arg => !arg.startsWith('--'));
    if (!args[0]) throw new Error('Usage: node scripts/Test-GameCompatibility.cjs <game.exe> [output.json] [--require-known]');
    const result = verify(args[0]);
    if (args[1]) fs.writeFileSync(args[1], JSON.stringify(result, null, 2) + '\n');
    const passed = result.staticChecksPassed && (!process.argv.includes('--require-known') || result.releaseBaselineKnown);
    console.log(`${passed ? 'PASS' : 'FAIL'}: structural bindings; ${Object.values(result.candidateCounts).filter(count => count === 1).length}/${Object.keys(result.candidateCounts).length} unique sites; ${result.releaseBaselineKnown ? 'known build ' + result.gameSteamBuild : 'unknown EXE hash'}. ${result.error || result.note}`);
    if (!passed) process.exitCode = 1;
}
module.exports = {verify};
