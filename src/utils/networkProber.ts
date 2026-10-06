import type { NetworkProbeResult } from '../types/diagnostics';

export async function probeNetworkAndRange(url: string): Promise<NetworkProbeResult> {
  const start = performance.now();
  const isHttpsPage = typeof window !== 'undefined' && window.location.protocol === 'https:';
  const isHttpTarget = url.startsWith('http:');

  const baseResult: NetworkProbeResult = {
    reachable: false,
    testedAt: Date.now(),
    httpStatus: null,
    statusText: null,
    acceptRanges: null,
    contentLength: null,
    contentRange: null,
    contentType: null,
    corsAllowed: false,
    mixedContentBlocked: isHttpsPage && isHttpTarget,
    latencyMs: 0,
  };

  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 8000);

    const res = await fetch(url, {
      method: 'GET',
      headers: {
        Range: 'bytes=0-1023',
      },
      signal: controller.signal,
    });
    clearTimeout(timeoutId);

    const latency = Math.round(performance.now() - start);

    const acceptRanges = res.headers.get('accept-ranges');
    const contentRange = res.headers.get('content-range');
    const contentLength = res.headers.get('content-length');
    const contentType = res.headers.get('content-type');

    return {
      ...baseResult,
      reachable: true,
      httpStatus: res.status,
      statusText: res.statusText,
      acceptRanges,
      contentRange,
      contentLength: contentLength ? parseInt(contentLength, 10) : null,
      contentType,
      corsAllowed: true, // Fetch completed successfully without CORS rejection
      latencyMs: latency,
    };
  } catch (err: unknown) {
    const latency = Math.round(performance.now() - start);
    const errMessage = err instanceof Error ? err.message : String(err);

    let specificError = errMessage;
    let mixedBlocked = baseResult.mixedContentBlocked;
    let corsIssue = false;

    if (errMessage.toLowerCase().includes('failed to fetch') || errMessage.toLowerCase().includes('networkerror')) {
      if (isHttpsPage && isHttpTarget) {
        mixedBlocked = true;
        specificError = 'Mixed Content Blocked: An HTTPS web app cannot directly fetch plain HTTP resources. The browser blocked the request.';
      } else {
        corsIssue = true;
        specificError = 'Network or CORS Error: The upstream media server either refused the connection or did not provide Access-Control-Allow-Origin headers.';
      }
    } else if (errMessage.toLowerCase().includes('aborted')) {
      specificError = 'Connection Timeout: Upstream server did not respond within 8 seconds.';
    }

    return {
      ...baseResult,
      reachable: false,
      corsAllowed: !corsIssue,
      mixedContentBlocked: mixedBlocked,
      latencyMs: latency,
      error: specificError,
    };
  }
}
