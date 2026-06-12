MW::COM Safety Report
======================

Safety documentation for the MW::COM component (Dependable Element, Integrity Level B).

.. raw:: html

   <iframe id="safety-report-frame" src="mw_com_doc/index.html"
      style="width:100%; height:1400px; border:1px solid #e0e0e0; border-radius:4px;"
      title="MW::COM Safety Report"></iframe>

.. raw:: html

    <script>
    (function() {
       var attempts = 0;
       var maxAttempts = 40;

       function routeToFrame(hash, frame) {
          if (!hash || hash.charAt(0) !== "#") return false;
          try {
             if (frame.contentWindow && frame.contentWindow.location) {
                frame.contentWindow.location.hash = hash;
                return true;
             }
          } catch (e) {
             // Fall back to updating src when iframe hash access is blocked.
          }
          var base = (frame.getAttribute("src") || "").replace(/#.*$/, "");
          frame.setAttribute("src", base + hash);
          return true;
       }

       function upsertFallbackSidebar(sidebar, frame) {
          var existing = document.getElementById("safety-right-links");
          if (existing) {
             existing.remove();
          }

          var wrapper = document.createElement("div");
          wrapper.className = "sidebar-secondary-item";
          wrapper.id = "safety-right-links";

          var list = document.createElement("ul");
          list.className = "visible nav section-nav flex-column";

          [
             ["Assumed System", "#assumed-system"],
             ["Software Architectural Level", "#software-architectural-level"],
             ["Components", "#components"],
             ["Checklists", "#checklists"],
             ["Submodules", "#submodules"]
          ].forEach(function(item) {
             var li = document.createElement("li");
             li.className = "toc-entry nav-item toc-h2";
             var a = document.createElement("a");
             a.className = "reference internal nav-link";
             a.setAttribute("href", "javascript:void(0)");
             a.setAttribute("data-hash", item[1]);
             a.textContent = item[0];
             a.addEventListener("click", function(ev) {
                ev.preventDefault();
                ev.stopPropagation();
                routeToFrame(item[1], frame);
             });
             li.appendChild(a);
             list.appendChild(li);
          });

          wrapper.appendChild(list);
          var sourceBlock = sidebar.querySelector(".tocsection.sourcelink");
          var sourceItem = sourceBlock ? sourceBlock.closest(".sidebar-secondary-item") : null;
          if (sourceItem) {
             sidebar.insertBefore(wrapper, sourceItem);
          } else {
             sidebar.appendChild(wrapper);
          }

          wrapper.addEventListener("click", function(ev) {
             var aEl = ev.target.closest("a");
             if (!aEl) return;
             ev.preventDefault();
             ev.stopPropagation();
             var hash = aEl.getAttribute("data-hash") || "";
             routeToFrame(hash, frame);
          });
       }

       function addSidebarLinks() {
          var sidebar = document.querySelector("#pst-secondary-sidebar .sidebar-secondary__inner");
          if (!sidebar) return;
          var frame = document.getElementById("safety-report-frame");
          if (!frame) return;

          var frameDoc;
          try {
             frameDoc = frame.contentDocument || frame.contentWindow.document;
          } catch (e) {
             upsertFallbackSidebar(sidebar, frame);
             return;
          }
          if (!frameDoc) {
             upsertFallbackSidebar(sidebar, frame);
             return;
          }

          var sourceToc = frameDoc.querySelector("#pst-secondary-sidebar .section-nav");
          if (!sourceToc) {
             attempts += 1;
             if (attempts < maxAttempts) {
                setTimeout(addSidebarLinks, 150);
             } else {
                upsertFallbackSidebar(sidebar, frame);
             }
             return;
          }

          var existing = document.getElementById("safety-right-links");
          if (existing) {
             existing.remove();
          }

          var wrapper = document.createElement("div");
          wrapper.className = "sidebar-secondary-item";
          wrapper.id = "safety-right-links";
          var list = sourceToc.cloneNode(true);
          list.classList.add("visible");
          list.querySelectorAll("a").forEach(function(link) {
             var href = link.getAttribute("href") || "";
             var hashIndex = href.indexOf("#");
             var hash = hashIndex >= 0 ? href.slice(hashIndex) : "";
             if (!hash && href.startsWith("#")) {
                hash = href;
             }
             link.setAttribute("data-hash", hash);
             link.setAttribute("href", "javascript:void(0)");
             link.addEventListener("click", function(ev) {
                ev.preventDefault();
                ev.stopPropagation();
                routeToFrame(hash, frame);
             });
          });

          wrapper.appendChild(list);

          var sourceBlock = sidebar.querySelector(".tocsection.sourcelink");
          var sourceItem = sourceBlock ? sourceBlock.closest(".sidebar-secondary-item") : null;
          if (sourceItem) {
             sidebar.insertBefore(wrapper, sourceItem);
          } else {
             sidebar.appendChild(wrapper);
          }

          wrapper.addEventListener("click", function(ev) {
             var aEl = ev.target.closest("a");
             if (!aEl) return;
             ev.preventDefault();
             ev.stopPropagation();
             var hash = aEl.getAttribute("data-hash") || "";
             routeToFrame(hash, frame);
          });
       }

       function init() {
          var frame = document.getElementById("safety-report-frame");
          if (!frame) return;
          document.addEventListener("click", function(ev) {
             var sidebar = document.getElementById("pst-secondary-sidebar");
             if (!sidebar) return;
             var link = ev.target.closest("#pst-secondary-sidebar .section-nav a");
             if (!link) return;
             ev.preventDefault();
             ev.stopPropagation();
             routeToFrame(link.getAttribute("data-hash") || link.getAttribute("href") || "", frame);
          }, true);
          frame.addEventListener("load", function() {
             attempts = 0;
             addSidebarLinks();
          });
          attempts = 0;
          addSidebarLinks();
       }

       if (document.readyState === "loading") {
          document.addEventListener("DOMContentLoaded", init);
       } else {
          init();
       }
    })();
    </script>
