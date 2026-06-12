Safety Reports
==============

Safety documentation reports

.. raw:: html

   <div id="safety-report-selector" style="margin-bottom:1rem;">
   <table class="table" style="width:100%; margin-bottom:0; border-collapse:collapse;">
     <thead>
       <tr>
         <th style="text-align:left; border-bottom:1px solid #d0d7de; padding:0.5rem;">Report</th>
         <th style="text-align:left; border-bottom:1px solid #d0d7de; padding:0.5rem;">Description</th>
       </tr>
     </thead>
     <tbody>
       <tr>
         <td style="padding:0.5rem; border-bottom:1px solid #d0d7de;">
           <button type="button" class="safety-report-link" data-report="mw_com_doc/index.html" style="background:none; border:none; padding:0; color:#0a6c8e; text-decoration:underline; cursor:pointer; font:inherit;">
             MW::COM
           </button>
         </td>
         <td style="padding:0.5rem; border-bottom:1px solid #d0d7de;">Dependable Element</td>
       </tr>
       <tr>
         <td style="padding:0.5rem; border-bottom:1px solid #d0d7de;">
           <button type="button" class="safety-report-link" data-report="dependable_element_message_passing_doc/index.html" style="background:none; border:none; padding:0; color:#0a6c8e; text-decoration:underline; cursor:pointer; font:inherit;">
             Message Passing
           </button>
         </td>
         <td style="padding:0.5rem; border-bottom:1px solid #d0d7de;">Dependable Element</td>
       </tr>
     </tbody>
   </table>
   </div>

.. raw:: html

  <iframe id="safety-main-report" src="about:blank"
      style="width:100%; height:1400px; border:1px solid #e0e0e0; border-radius:4px;"
      title="Safety Reports"></iframe>

   <script>
   (function() {
     var attempts = 0;
     var maxAttempts = 40;
     var currentReport = "";

     function buildFallbackItems(report) {
       if (report.indexOf("dependable_element_message_passing_doc") >= 0) {
         return [
           ["#assumed-system", "Assumed System"],
           ["#software-architectural-level", "Software Architectural Level"],
           ["#components", "Components"],
           ["#checklists", "Checklists"],
           ["#submodules", "Submodules"],
         ];
       }

       return [
         ["#assumed-system", "Assumed System"],
         ["#software-architectural-level", "Software Architectural Level"],
         ["#components", "Components"],
         ["#checklists", "Checklists"],
         ["#submodules", "Submodules"],
       ];
     }

     function upsertFallbackRightSidebar(sidebar, frame, report) {
       var existing = document.getElementById("safety-right-links");
       if (existing) {
         existing.remove();
       }

       var wrapper = document.createElement("div");
       wrapper.className = "sidebar-secondary-item";
       wrapper.id = "safety-right-links";
       var list = document.createElement("ul");
       list.className = "visible nav section-nav flex-column";

       buildFallbackItems(report).forEach(function(item) {
         var li = document.createElement("li");
         li.className = "toc-entry nav-item toc-h2";
         var link = document.createElement("a");
         link.className = "reference internal nav-link";
         link.setAttribute("href", "javascript:void(0)");
         link.setAttribute("data-hash", item[0]);
         link.textContent = item[1];
         link.addEventListener("click", function(ev) {
           ev.preventDefault();
           ev.stopPropagation();
           var base = (frame.getAttribute("src") || "").replace(/#.*$/, "");
           frame.setAttribute("src", base + item[0]);
         });
         li.appendChild(link);
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
     }

     function setReport(src) {
       var frame = document.getElementById("safety-main-report");
       var selector = document.getElementById("safety-report-selector");
       if (!frame) return;
       currentReport = src;
       if (selector) {
         selector.style.display = "none";
       }
       frame.style.display = "block";
       frame.setAttribute("src", src);
     }

     function syncRightSidebar() {
       var sidebar = document.querySelector("#pst-secondary-sidebar .sidebar-secondary__inner");
       var frame = document.getElementById("safety-main-report");
       if (!sidebar || !frame) return;

       var frameDoc;
       try {
         frameDoc = frame.contentDocument || frame.contentWindow.document;
       } catch (e) {
         upsertFallbackRightSidebar(sidebar, frame, currentReport || frame.getAttribute("src") || "");
         return;
       }
       if (!frameDoc) {
         upsertFallbackRightSidebar(sidebar, frame, currentReport || frame.getAttribute("src") || "");
         return;
       }

       var sourceToc = frameDoc.querySelector("#pst-secondary-sidebar .section-nav");
       if (!sourceToc) {
         attempts += 1;
         if (attempts < maxAttempts) {
           setTimeout(syncRightSidebar, 150);
         } else {
           upsertFallbackRightSidebar(sidebar, frame, currentReport || frame.getAttribute("src") || "");
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
         var hash = href.indexOf("#") >= 0 ? href.slice(href.indexOf("#")) : "";
         link.setAttribute("href", "javascript:void(0)");
         link.setAttribute("data-hash", hash);
         link.addEventListener("click", function(ev) {
           ev.preventDefault();
           ev.stopPropagation();
           var base = frameSrc.replace(/#.*$/, "");
           if (hash) {
             frame.setAttribute("src", base + hash);
           }
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
     }

     function init() {
       var frame = document.getElementById("safety-main-report");
       if (!frame) return;

       frame.style.display = "none";

       document.querySelectorAll(".safety-report-link").forEach(function(link) {
         link.addEventListener("click", function(ev) {
           ev.preventDefault();
           var report = link.getAttribute("data-report") || "";
           if (!report) return;
           attempts = 0;
           setReport(report);
         });
       });

       frame.addEventListener("load", function() {
         attempts = 0;
         syncRightSidebar();
       });
     }

     if (document.readyState === "loading") {
       document.addEventListener("DOMContentLoaded", init);
     } else {
       init();
     }
   })();
   </script>

.. toctree::
   :hidden:

   safety_message_passing
   safety_mw_com
