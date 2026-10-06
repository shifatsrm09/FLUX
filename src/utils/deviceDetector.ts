import type { DeviceInfo } from '../types/diagnostics';

export async function detectDevice(): Promise<DeviceInfo> {
  const ua = navigator.userAgent;

  // OS Detection
  const isIOS = /iPhone|iPad|iPod/i.test(ua) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  const isAndroid = /Android/i.test(ua);
  const isWindows = /Windows/i.test(ua);
  const isMac = /Macintosh|Mac OS X/i.test(ua) && !isIOS;
  const isLinux = /Linux/i.test(ua) && !isAndroid;

  let osName = 'Unknown OS';
  if (isIOS) osName = 'Apple iOS';
  else if (isAndroid) osName = 'Android';
  else if (isWindows) osName = 'Windows';
  else if (isMac) osName = 'macOS';
  else if (isLinux) osName = 'Linux';

  // Mobile Detection
  const isMobile = isIOS || isAndroid || /Mobi|Tablet/i.test(ua);

  // Browser Detection
  let isBrave = false;
  try {
    if ((navigator as unknown as { brave?: { isBrave?: () => Promise<boolean> } }).brave?.isBrave) {
      isBrave = await (navigator as unknown as { brave: { isBrave: () => Promise<boolean> } }).brave.isBrave();
    }
  } catch {
    isBrave = false;
  }

  const isFirefox = /Firefox|FxiOS/i.test(ua);
  const isEdge = /Edg/i.test(ua);
  const isChromeOrChromium = !isFirefox && !isEdge && (/Chrome|CriOS/i.test(ua) || isBrave);
  const isSafari = !isChromeOrChromium && !isFirefox && !isEdge && /Safari/i.test(ua);

  let browserName = 'Browser';
  let browserVersion = '';

  if (isBrave) {
    browserName = 'Brave';
  } else if (isEdge) {
    browserName = 'Microsoft Edge';
    const match = ua.match(/Edg\/([\d.]+)/);
    if (match) browserVersion = match[1];
  } else if (isChromeOrChromium) {
    browserName = 'Google Chrome / Chromium';
    const match = ua.match(/Chrome\/([\d.]+)/);
    if (match) browserVersion = match[1];
  } else if (isFirefox) {
    browserName = 'Mozilla Firefox';
    const match = ua.match(/Firefox\/([\d.]+)/);
    if (match) browserVersion = match[1];
  } else if (isSafari) {
    browserName = 'Apple Safari';
    const match = ua.match(/Version\/([\d.]+)/);
    if (match) browserVersion = match[1];
  }

  const hasMediaSource = typeof window !== 'undefined' && 'MediaSource' in window;
  const hasManagedMediaSource = typeof window !== 'undefined' && 'ManagedMediaSource' in window;
  const hasWebAudio = typeof window !== 'undefined' && ('AudioContext' in window || 'webkitAudioContext' in window);

  return {
    userAgent: ua,
    browserName,
    browserVersion,
    osName,
    isMobile,
    isIOS,
    isAndroid,
    isSafari,
    isChromeOrChromium,
    isBrave,
    isFirefox,
    hasMediaSource,
    hasManagedMediaSource,
    hasWebAudio,
  };
}
