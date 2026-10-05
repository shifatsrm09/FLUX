const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const TEST_PORT = 3099;
const UPSTREAM_PORT = 8099;

function makeRequest(options, postData = null) {
  return new Promise((resolve, reject) => {
    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', (chunk) => { data += chunk; });
      res.on('end', () => {
        resolve({
          statusCode: res.statusCode,
          headers: res.headers,
          body: data,
        });
      });
    });
    req.on('error', reject);
    if (postData) {
      req.write(typeof postData === 'string' ? postData : JSON.stringify(postData));
    }
    req.end();
  });
}

async function run() {
  console.log('=== STARTING BDIXSTREAM HLS VOD TEST SUITE ===');

  // 1. Generate 60-second test MKV (10 segments @ 6s each)
  const testMkvPath = path.join(__dirname, 'test_vod_60s.mkv');
  console.log('Generating 60-second test MKV...');
  await new Promise((resolve, reject) => {
    const ff = spawn('ffmpeg', [
      '-f', 'lavfi', '-i', 'testsrc=duration=60:size=640x360:rate=24',
      '-f', 'lavfi', '-i', 'sine=duration=60:frequency=440',
      '-c:v', 'libx264', '-g', '48', '-c:a', 'aac',
      '-y', testMkvPath,
    ]);
    ff.on('close', (code) => (code === 0 ? resolve() : reject(new Error('ffmpeg failed'))));
  });
  console.log('Generated test MKV:', testMkvPath);

  // 2. Start mock upstream HTTP media server on port 8099 with HTTP Range support
  const mockServer = http.createServer((req, res) => {
    if (req.url === '/test_vod_60s.mkv') {
      const stat = fs.statSync(testMkvPath);
      const fileSize = stat.size;
      const range = req.headers.range;

      if (range) {
        const parts = range.replace(/bytes=/, '').split('-');
        const start = parseInt(parts[0], 10);
        const end = parts[1] ? parseInt(parts[1], 10) : fileSize - 1;
        const chunksize = end - start + 1;

        res.writeHead(206, {
          'Content-Range': `bytes ${start}-${end}/${fileSize}`,
          'Accept-Ranges': 'bytes',
          'Content-Length': chunksize,
          'Content-Type': 'video/x-matroska',
        });
        fs.createReadStream(testMkvPath, { start, end }).pipe(res);
      } else {
        res.writeHead(200, {
          'Content-Length': fileSize,
          'Accept-Ranges': 'bytes',
          'Content-Type': 'video/x-matroska',
        });
        fs.createReadStream(testMkvPath).pipe(res);
      }
    } else {
      res.writeHead(404);
      res.end();
    }
  });

  await new Promise((resolve) => mockServer.listen(UPSTREAM_PORT, resolve));
  console.log(`Mock upstream server listening on http://127.0.0.1:${UPSTREAM_PORT}`);

  const mediaUrl = `http://127.0.0.1:${UPSTREAM_PORT}/test_vod_60s.mkv`;

  // 3. Start BDIXStream backend server on test port
  const serverProcess = spawn('node', ['dist/server.js'], {
    cwd: __dirname,
    stdio: ['ignore', 'pipe', 'pipe'],
    env: { ...process.env, PORT: String(TEST_PORT) },
  });

  serverProcess.stdout.on('data', (d) => process.stdout.write('[SERVER] ' + d));
  serverProcess.stderr.on('data', (d) => process.stderr.write('[SERVER ERR] ' + d));

  // Wait for server to listen
  await new Promise((resolve) => setTimeout(resolve, 2500));

  try {
    // TEST 1: Health check
    console.log('\n--- TEST 1: Health check ---');
    const health = await makeRequest({ hostname: 'localhost', port: TEST_PORT, path: '/api/health', method: 'GET' });
    console.log('Status:', health.statusCode, 'Body:', health.body);
    if (health.statusCode !== 200) throw new Error('Health check failed');

    // TEST 2: Invalid URL validation
    console.log('\n--- TEST 2: Invalid URL rejected ---');
    const invalidUrlRes = await makeRequest(
      {
        hostname: 'localhost',
        port: TEST_PORT,
        path: '/api/hls/session',
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
      },
      { url: 'file:///etc/passwd' }
    );
    console.log('Status:', invalidUrlRes.statusCode, 'Body:', invalidUrlRes.body);
    if (invalidUrlRes.statusCode !== 400) throw new Error('Invalid URL was not rejected');

    // TEST 3: Invalid session ID rejected
    console.log('\n--- TEST 3: Invalid session ID rejected ---');
    const invalidSession = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: '/api/hls/invalid-session-id/index.m3u8',
      method: 'GET',
    });
    console.log('Status:', invalidSession.statusCode);
    if (invalidSession.statusCode !== 400) throw new Error('Invalid session format was not rejected');

    // TEST 4: Path traversal rejected
    console.log('\n--- TEST 4: Path traversal rejected ---');
    const traversal = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: '/api/hls/123e4567-e89b-12d3-a456-426614174000/..%2f..%2fetc%2fpasswd',
      method: 'GET',
    });
    console.log('Status:', traversal.statusCode);
    if (traversal.statusCode !== 400 && traversal.statusCode !== 404) throw new Error('Path traversal not blocked');

    // TEST 5: Create HLS Session
    console.log('\n--- TEST 5: Create HLS Session ---');
    const createSessionRes = await makeRequest(
      {
        hostname: 'localhost',
        port: TEST_PORT,
        path: '/api/hls/session',
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
      },
      { url: mediaUrl }
    );
    console.log('Create Session Status:', createSessionRes.statusCode);
    const sessionData = JSON.parse(createSessionRes.body);
    console.log('Session response:', sessionData);

    if (createSessionRes.statusCode !== 200 || !sessionData.sessionId) {
      throw new Error('Failed to create HLS session');
    }
    const sessionId = sessionData.sessionId;

    // TEST 6: Get index.m3u8
    console.log('\n--- TEST 6: Get index.m3u8 playlist ---');
    const playlistRes = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/index.m3u8`,
      method: 'GET',
    });
    console.log('Playlist Status:', playlistRes.statusCode);
    console.log('Playlist Headers:', playlistRes.headers['content-type']);
    console.log('Playlist Preview:\n' + playlistRes.body);

    if (!playlistRes.body.includes('#EXT-X-PLAYLIST-TYPE:VOD')) {
      throw new Error('Playlist missing VOD tag');
    }
    if (!playlistRes.body.includes('#EXT-X-ENDLIST')) {
      throw new Error('Playlist missing ENDLIST tag');
    }
    if (!playlistRes.body.includes('init.mp4')) {
      throw new Error('Playlist missing init.mp4 tag');
    }

    // TEST 7: Get init.mp4
    console.log('\n--- TEST 7: Get init.mp4 ---');
    const initRes = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/init.mp4`,
      method: 'GET',
    });
    console.log('init.mp4 Status:', initRes.statusCode, 'Content-Length:', initRes.body.length);
    if (initRes.statusCode !== 200 || initRes.body.length === 0) {
      throw new Error('init.mp4 failed');
    }

    // TEST 8: Get segment_0000.m4s
    console.log('\n--- TEST 8: Request segment_0000.m4s (start) ---');
    const seg0Res = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/segment_0000.m4s`,
      method: 'GET',
    });
    console.log('segment_0000 Status:', seg0Res.statusCode, 'Bytes:', seg0Res.body.length);
    if (seg0Res.statusCode !== 200 || seg0Res.body.length === 0) {
      throw new Error('segment_0000 failed');
    }

    // TEST 9: Arbitrary Seek to 50% (segment_0005.m4s = 30s)
    console.log('\n--- TEST 9: Arbitrary seek to 50% (segment_0005.m4s) ---');
    const tStart50 = Date.now();
    const seg5Res = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/segment_0005.m4s`,
      method: 'GET',
    });
    const tElapsed50 = Date.now() - tStart50;
    console.log(`segment_0005 Status: ${seg5Res.statusCode}, Bytes: ${seg5Res.body.length}, Elapsed: ${tElapsed50}ms`);
    if (seg5Res.statusCode !== 200 || seg5Res.body.length === 0) {
      throw new Error('Seek to 50% failed');
    }

    // TEST 10: Arbitrary Seek to 90% (segment_0009.m4s = 54s)
    console.log('\n--- TEST 10: Arbitrary seek to 90% (segment_0009.m4s) ---');
    const tStart90 = Date.now();
    const seg9Res = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/segment_0009.m4s`,
      method: 'GET',
    });
    const tElapsed90 = Date.now() - tStart90;
    console.log(`segment_0009 Status: ${seg9Res.statusCode}, Bytes: ${seg9Res.body.length}, Elapsed: ${tElapsed90}ms`);
    if (seg9Res.statusCode !== 200 || seg9Res.body.length === 0) {
      throw new Error('Seek to 90% failed');
    }

    // TEST 11: Concurrent Neighboring Segments (hls.js pattern: segments 2, 3, 4 requested simultaneously)
    console.log('\n--- TEST 11: Concurrent neighboring segment requests (2, 3, 4 simultaneously) ---');
    const tStartConc = Date.now();
    const [seg2Res, seg3Res, seg4Res] = await Promise.all([
      makeRequest({ hostname: 'localhost', port: TEST_PORT, path: `/api/hls/${sessionId}/segment_0002.m4s`, method: 'GET' }),
      makeRequest({ hostname: 'localhost', port: TEST_PORT, path: `/api/hls/${sessionId}/segment_0003.m4s`, method: 'GET' }),
      makeRequest({ hostname: 'localhost', port: TEST_PORT, path: `/api/hls/${sessionId}/segment_0004.m4s`, method: 'GET' }),
    ]);
    const tElapsedConc = Date.now() - tStartConc;
    console.log(`Concurrent fetch finished in ${tElapsedConc}ms:`);
    console.log(`  segment_0002 Status: ${seg2Res.statusCode}, Bytes: ${seg2Res.body.length}`);
    console.log(`  segment_0003 Status: ${seg3Res.statusCode}, Bytes: ${seg3Res.body.length}`);
    console.log(`  segment_0004 Status: ${seg4Res.statusCode}, Bytes: ${seg4Res.body.length}`);

    if (seg2Res.statusCode !== 200 || seg2Res.body.length === 0 ||
        seg3Res.statusCode !== 200 || seg3Res.body.length === 0 ||
        seg4Res.statusCode !== 200 || seg4Res.body.length === 0) {
      throw new Error('Concurrent neighboring segment requests failed');
    }

    // TEST 12: Request Deduplication (Two identical simultaneous requests for uncached segment 7)
    console.log('\n--- TEST 12: Request deduplication (simultaneous requests for segment_0007.m4s) ---');
    const [seg7A, seg7B] = await Promise.all([
      makeRequest({ hostname: 'localhost', port: TEST_PORT, path: `/api/hls/${sessionId}/segment_0007.m4s`, method: 'GET' }),
      makeRequest({ hostname: 'localhost', port: TEST_PORT, path: `/api/hls/${sessionId}/segment_0007.m4s`, method: 'GET' }),
    ]);
    console.log(`  seg7A Status: ${seg7A.statusCode}, Bytes: ${seg7A.body.length}`);
    console.log(`  seg7B Status: ${seg7B.statusCode}, Bytes: ${seg7B.body.length}`);
    if (seg7A.statusCode !== 200 || seg7B.statusCode !== 200 || seg7A.body.length !== seg7B.body.length) {
      throw new Error('Identical concurrent segment request deduplication failed');
    }

    // TEST 13: Backward seek back to 10% (segment_0001.m4s = 6s)
    console.log('\n--- TEST 13: Backward seek to 10% (segment_0001.m4s) ---');
    const tStart10 = Date.now();
    const seg1Res = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/segment_0001.m4s`,
      method: 'GET',
    });
    const tElapsed10 = Date.now() - tStart10;
    console.log(`segment_0001 Status: ${seg1Res.statusCode}, Bytes: ${seg1Res.body.length}, Elapsed: ${tElapsed10}ms`);
    if (seg1Res.statusCode !== 200 || seg1Res.body.length === 0) {
      throw new Error('Backward seek failed');
    }

    // TEST 14: Cached seek hit (segment_0005.m4s should be instant)
    console.log('\n--- TEST 14: Cached seek hit (segment_0005.m4s) ---');
    const tStartCache = Date.now();
    const seg5Cache = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/segment_0005.m4s`,
      method: 'GET',
    });
    const tElapsedCache = Date.now() - tStartCache;
    console.log(`segment_0005 cached Status: ${seg5Cache.statusCode}, Elapsed: ${tElapsedCache}ms`);
    if (seg5Cache.statusCode !== 200) throw new Error('Cached seek failed');

    // TEST 15: Check session status and multi-worker reporting
    console.log('\n--- TEST 15: Check session status ---');
    const statusRes = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/status`,
      method: 'GET',
    });
    console.log('Status diagnostics:', statusRes.body);
    const statusData = JSON.parse(statusRes.body);
    if (!Array.isArray(statusData.workers)) {
      throw new Error('Status response missing workers array');
    }

    // TEST 16: Delete session
    console.log('\n--- TEST 16: Delete session & verify cleanup ---');
    const delRes = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}`,
      method: 'DELETE',
    });
    console.log('Delete response:', delRes.body);

    // Verify session is now 404
    const afterDelete = await makeRequest({
      hostname: 'localhost',
      port: TEST_PORT,
      path: `/api/hls/${sessionId}/index.m3u8`,
      method: 'GET',
    });
    console.log('Post-delete playlist check (should be 404):', afterDelete.statusCode);
    if (afterDelete.statusCode !== 404) throw new Error('Session was not properly removed');

    console.log('\n=========================================');
    console.log('>>> ALL 16 TEST SUITE CHECKS PASSED! <<<');
    console.log('=========================================');
  } finally {
    // Cleanup
    mockServer.close();
    serverProcess.kill('SIGINT');
    try { fs.unlinkSync(testMkvPath); } catch {}
  }
}

run().catch((err) => {
  console.error('\n❌ TEST SUITE FAILURE:', err);
  process.exit(1);
});
