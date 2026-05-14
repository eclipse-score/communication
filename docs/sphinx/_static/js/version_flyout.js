/**
 * RTD-style version flyout for GitHub Pages.
 * Reads versions from switcher.json and renders a floating panel.
 */
(function () {
  "use strict";

  // Determine base URL and current version from the URL path
  var pathParts = window.location.pathname.split("/").filter(Boolean);
  // Expect: /communication/<version>/...
  var repoName = pathParts[0] || "";
  var currentVersion = pathParts[1] || "latest";
  // Handle preview/<branch> pattern
  if (currentVersion === "preview" && pathParts[2]) {
    currentVersion = "preview/" + pathParts[2];
  }

  var baseUrl =
    window.location.origin + "/" + repoName;
  var switcherUrl = baseUrl + "/switcher.json";

  function createFlyout(versions) {
    var container = document.createElement("div");
    container.className = "version-flyout";

    // Build version links
    var versionLinks = versions
      .map(function (v) {
        var activeClass = v.version === currentVersion ? ' class="active"' : "";
        return '<a href="' + v.url + '"' + activeClass + ">" + v.name + "</a>";
      })
      .join("\n");

    container.innerHTML =
      '<div class="version-flyout__panel" id="flyout-panel">' +
      '  <div class="flyout-section">' +
      "    <h4>Versions</h4>" +
      '    <div class="flyout-versions">' +
      versionLinks +
      "    </div>" +
      "  </div>" +
      '  <div class="flyout-links">' +
      '    <a href="https://github.com/' + getGitHubRepo() + '">GitHub</a>' +
      '    <a href="' + baseUrl + '/latest/">Latest</a>' +
      "  </div>" +
      '  <div class="version-flyout__footer">' +
      "    Hosted on GitHub Pages" +
      "  </div>" +
      "</div>" +
      '<button class="version-flyout__toggle" id="flyout-toggle">' +
      '  <span class="flyout-icon">&#9733;</span>' +
      '  <span class="flyout-current">' + currentVersion + "</span>" +
      '  <span class="flyout-arrow">&#9650;</span>' +
      "</button>";

    document.body.appendChild(container);

    // Toggle behavior
    var toggle = document.getElementById("flyout-toggle");
    var panel = document.getElementById("flyout-panel");
    toggle.addEventListener("click", function () {
      panel.classList.toggle("open");
      toggle.classList.toggle("active");
    });
  }

  function getGitHubRepo() {
    // Try to extract from meta or fallback
    var metaRepo = document.querySelector('meta[name="github-repo"]');
    if (metaRepo) return metaRepo.getAttribute("content");
    // Fallback based on GitHub Pages domain
    var host = window.location.hostname;
    var user = host.split(".")[0];
    return user + "/" + repoName;
  }

  // Fetch switcher.json and build the flyout
  fetch(switcherUrl)
    .then(function (response) {
      if (!response.ok) throw new Error("switcher.json not found");
      return response.json();
    })
    .then(function (versions) {
      createFlyout(versions);
    })
    .catch(function (err) {
      console.warn("Version flyout: Could not load switcher.json", err);
    });
})();
