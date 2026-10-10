const releaseEndpoint = "https://api.github.com/repos/shifatsrm09/FLUX/releases/latest";
const setupLink = document.querySelector("#setup-download");
const portableLink = document.querySelector("#portable-download");
const note = document.querySelector("#release-note");
const status = document.querySelector("#release-status");

async function loadLatestRelease() {
  try {
    const response = await fetch(releaseEndpoint, {
      headers: { Accept: "application/vnd.github+json" },
      cache: "no-store",
    });
    if (!response.ok) throw new Error(`GitHub returned ${response.status}`);

    const release = await response.json();
    const assets = release.assets ?? [];
    const setup = assets.find((asset) => /-Setup\.exe$/i.test(asset.name));
    const portable = assets.find((asset) => /-win64-portable\.zip$/i.test(asset.name));

    if (setup) setupLink.href = setup.browser_download_url;
    if (portable) portableLink.href = portable.browser_download_url;
    if (!setup || !portable) throw new Error("One or more download assets were not found");

    const version = release.tag_name || release.name || "Latest";
    note.textContent = `${version} · ${formatSize(setup.size)} installer · ${formatSize(portable.size)} portable`;
    status.textContent = "THE LATEST RELEASE";
  } catch (error) {
    // The releases page remains a working fallback if the API is unavailable.
    note.textContent = "Latest downloads on GitHub · Windows 10/11";
    status.textContent = "READY TO DOWNLOAD";
  }
}

function formatSize(bytes) {
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

loadLatestRelease();
