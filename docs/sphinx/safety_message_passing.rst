Message Passing Safety Report
==============================

Safety documentation for the Message Passing component (Dependable Element, Integrity Level B).

.. raw:: html

   <iframe id="safety-report-frame" src="dependable_element_message_passing_doc/index.html"
      style="width:100%; height:1400px; border:1px solid #e0e0e0; border-radius:4px;"
      title="Message Passing Safety Report"></iframe>

.. raw:: html

    <script>
    (function() {
       var attempts = 0;
       var maxAttempts = 40;

       function routeToFrame(href, frame) {
          var hashIndex = href.indexOf("#");
          if (hashIndex < 0) return false;
          var hash = href.slice(hashIndex);
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
             ["Assumed System", "dependable_element_message_passing_doc/index.html#assumed-system"],
             ["Software Architectural Level", "dependable_element_message_passing_doc/index.html#software-architectural-level"],
             ["Components", "dependable_element_message_passing_doc/index.html#components"],
             ["Checklists", "dependable_element_message_passing_doc/index.html#checklists"],
             ["Submodules", "dependable_element_message_passing_doc/index.html#submodules"]
          ].forEach(function(item) {
             var li = document.createElement("li");
             li.className = "toc-entry nav-item toc-h2";
             var a = document.createElement("a");
             a.className = "reference internal nav-link";
             a.setAttribute("href", item[1]);
             a.textContent = item[0];
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
             var href = aEl.getAttribute("href") || "";
             if (routeToFrame(href, frame)) {
                ev.preventDefault();
             }
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
          var frameSrc = frame.getAttribute("src") || "";

          list.querySelectorAll("a").forEach(function(link) {
             var href = link.getAttribute("href") || "";
             if (href.startsWith("#")) {
                href = frameSrc.replace(/#.*$/, "") + href;
             }
             link.setAttribute("href", href);
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
             var href = aEl.getAttribute("href") || "";
             if (routeToFrame(href, frame)) {
                ev.preventDefault();
             }
          });
       }

       function init() {
          var frame = document.getElementById("safety-report-frame");
          if (!frame) return;
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
