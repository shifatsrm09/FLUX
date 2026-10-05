/**
 * Validate that a string is a well-formed HTTP or HTTPS URL
 * suitable for passing to FFprobe.
 *
 * Rejects file://, ftp://, and other schemes to avoid
 * unintended filesystem or network access.
 */
export function isValidMediaUrl(url: string): boolean {
  try {
    const parsed = new URL(url);
    return parsed.protocol === 'http:' || parsed.protocol === 'https:';
  } catch {
    return false;
  }
}
