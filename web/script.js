const releaseEndpoint = "https://api.github.com/repos/shifatsrm09/FLUX/releases/latest";
const setupLink = document.querySelector("#setup-download");
const portableLink = document.querySelector("#portable-download");
const networkDownloadLink = document.querySelector("#network-check-download");
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

    if (setup) {
      setupLink.href = setup.browser_download_url;
      if (networkDownloadLink) networkDownloadLink.href = setup.browser_download_url;
    }
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

const networkCheckDialog = document.querySelector("#network-check-dialog");
const networkCheckIntro = document.querySelector("#network-check-intro");
const networkCheckPanel = document.querySelector("#network-check-panel");
const networkDialogTitle = document.querySelector("#network-dialog-title");
const networkCheckResults = document.querySelector("#network-check-results");
const networkCheckResult = document.querySelector("#network-check-result");
const networkCheckIndicator = document.querySelector("#network-check-indicator");
const networkSources = [
  { url: "http://172.16.50.4/", resultId: "#source-result-gateway" },
];
const pendingNetworkSource = { resultId: "#source-result-dashboard" };
let activeNetworkChecks = [];
let networkCheckRun = 0;
let localNetworkPermission = "unknown";
getLocalNetworkPermission().then((permission) => {
  localNetworkPermission = permission;
});

document.querySelector("#network-check-open").addEventListener("click", () => {
  networkCheckIntro.hidden = false;
  networkCheckPanel.hidden = true;
  networkCheckResults.hidden = false;
  manualCheckPanel.hidden = true;
  guaranteedCheckButton.setAttribute("aria-expanded", "false");
  networkDialogTitle.textContent = "Before you check";
  if (!networkCheckDialog.open) networkCheckDialog.showModal();
});
document.querySelector("#network-check-proceed").addEventListener("click", () => {
  networkCheckIntro.hidden = true;
  networkCheckPanel.hidden = false;
  networkDialogTitle.textContent = "Can FLUX reach your network?";
  runNetworkCheck();
});
const guaranteedCheckButton = document.querySelector("#network-guaranteed-check");
const manualCheckPanel = document.querySelector("#network-manual-check");
guaranteedCheckButton.addEventListener("click", () => {
  networkCheckResults.hidden = true;
  manualCheckPanel.hidden = false;
  guaranteedCheckButton.setAttribute("aria-expanded", "true");
  networkDialogTitle.textContent = "Guaranteed Check";
});
document.querySelector("#network-check-back").addEventListener("click", () => {
  manualCheckPanel.hidden = true;
  networkCheckResults.hidden = false;
  guaranteedCheckButton.setAttribute("aria-expanded", "false");
  networkDialogTitle.textContent = "Can FLUX reach your network?";
});

document.querySelectorAll(".copy-network-link").forEach((button) => {
  button.addEventListener("click", async () => {
    const url = button.dataset.copyUrl;
    const copyStatus = document.querySelector("#manual-check-copy-status");
    try {
      if (!navigator.clipboard?.writeText) throw new Error("Clipboard unavailable");
      await navigator.clipboard.writeText(url);
    } catch {
      const field = document.createElement("textarea");
      field.value = url;
      field.setAttribute("readonly", "");
      field.style.position = "fixed";
      field.style.opacity = "0";
      document.body.append(field);
      field.select();
      const copied = document.execCommand("copy");
      field.remove();
      if (!copied) {
        copyStatus.textContent = "Couldn’t copy. Select the link and copy it manually.";
        return;
      }
    }

    const originalLabel = button.textContent;
    button.textContent = "Copied";
    copyStatus.textContent = `${button.closest(".manual-check-link").querySelector("strong").textContent} link copied.`;
    window.setTimeout(() => { button.textContent = originalLabel; }, 1400);
  });
});
document.querySelector("#network-check-close").addEventListener("click", () => {
  networkCheckDialog.close();
});

networkCheckDialog.addEventListener("close", () => {
  networkCheckRun += 1;
  activeNetworkChecks.forEach((controller) => controller.abort());
  activeNetworkChecks = [];
});

async function runNetworkCheck() {
  if (!networkCheckDialog.open) networkCheckDialog.showModal();

  const run = ++networkCheckRun;

  networkCheckResult.textContent = "Checking the DHAKA FLIX gateway…";
  networkCheckIndicator.dataset.state = "checking";
  networkSources.forEach((source) => setNetworkSourceResult(source, "checking", "Checking"));
  setNetworkSourceResult(pendingNetworkSource, "checking", "In progress");

  try {
    if (localNetworkPermission === "denied") {
      networkCheckResult.textContent = "Local network access is blocked for this site. Allow it in your browser’s site settings, then try again.";
      networkCheckIndicator.dataset.state = "failure";
      networkSources.forEach((source) => setNetworkSourceResult(source, "failure", "Blocked"));
      return;
    }

    const results = await Promise.all(networkSources.map((source) => checkNetworkSource(source, run)));
    if (run !== networkCheckRun) return;

    const reachableCount = results.filter((result) => result.reachable).length;
    if (reachableCount > 0) {
      networkCheckResult.textContent = "The DHAKA FLIX gateway is reachable. FLUX should work on this network; the ICC check is still in progress.";
      networkCheckIndicator.dataset.state = "success";
      return;
    }

    const permission = await getLocalNetworkPermission();
    localNetworkPermission = permission;
    if (permission === "denied") {
      networkCheckResult.textContent = "Local network access is blocked for this site. Allow it in your browser’s site settings, then try again.";
    } else if (permission === "unsupported") {
      networkCheckResult.textContent = "This browser can’t confirm local network access. Try the latest Chrome or Edge, then allow access if prompted.";
    } else if (permission === "prompt") {
      networkCheckResult.textContent = "Allow local network access for this site in your browser’s site settings, then reload and try again.";
    } else if (results.every((result) => result.timedOut)) {
      networkCheckResult.textContent = "The gateway didn’t respond. FLUX may still work; the ICC checker isn’t ready yet.";
    } else {
      networkCheckResult.textContent = "Your browser allows local network access, but the gateway didn’t respond. FLUX may still work; the ICC checker isn’t ready yet.";
    }
    networkCheckIndicator.dataset.state = "failure";
  } catch (error) {
    if (run === networkCheckRun) {
      const permission = await getLocalNetworkPermission();
      localNetworkPermission = permission;
      if (permission === "denied") {
        networkCheckResult.textContent = "Local network access is blocked for this site. Allow it in your browser’s site settings, then try again.";
      } else if (permission === "unsupported") {
        networkCheckResult.textContent = "This browser can’t confirm local network access. Try the latest Chrome or Edge, then allow access if prompted.";
      } else if (permission === "prompt") {
        networkCheckResult.textContent = "Allow local network access for this site in your browser’s site settings, then reload and try again.";
      } else if (activeNetworkChecks.length === 0) {
        networkCheckResult.textContent = "The gateway didn’t respond. FLUX may still work; the ICC checker isn’t ready yet.";
      } else if (permission === "granted") {
        networkCheckResult.textContent = "Your browser allows local network access, but the gateway didn’t respond. FLUX may still work; the ICC checker isn’t ready yet.";
      } else {
        networkCheckResult.textContent = "The browser blocked this local network check. Try the latest Chrome or Edge and allow access for this site.";
      }
      networkCheckIndicator.dataset.state = "failure";
    }
  } finally {
    if (run === networkCheckRun) {
      activeNetworkChecks = [];
    }
  }
}

async function checkNetworkSource(source, run) {
  if (localNetworkPermission === "denied") {
    setNetworkSourceResult(source, "failure", "Blocked");
    return { reachable: false, timedOut: false };
  }

  const controller = new AbortController();
  activeNetworkChecks.push(controller);
  const timeout = window.setTimeout(() => controller.abort(), 15000);

  try {
    await fetch(source.url, {
      method: "GET",
      mode: "no-cors",
      cache: "no-store",
      redirect: "follow",
      signal: controller.signal,
      targetAddressSpace: "local",
    });

    if (run === networkCheckRun) setNetworkSourceResult(source, "success", "Reachable");
    return { reachable: true, timedOut: false };
  } catch {
    if (run === networkCheckRun) setNetworkSourceResult(source, "failure", controller.signal.aborted ? "Timed out" : "Unavailable");
    return { reachable: false, timedOut: controller.signal.aborted };
  } finally {
    window.clearTimeout(timeout);
  }
}

function setNetworkSourceResult(source, state, label) {
  const result = document.querySelector(source.resultId);
  result.dataset.state = state;
  if (state === "success") {
    result.innerHTML = `<span class="source-result-check" aria-hidden="true">✓</span>${label}`;
  } else if (state === "failure") {
    result.innerHTML = `<span class="source-result-cross" aria-hidden="true">×</span>${label}`;
  } else {
    result.textContent = label;
  }
}

async function getLocalNetworkPermission() {
  if (!navigator.permissions?.query) return "unsupported";
  try {
    const permission = await navigator.permissions.query({ name: "local-network" });
    return permission.state;
  } catch {
    return "unsupported";
  }
}
