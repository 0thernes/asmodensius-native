// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Alexander Stephen Donahue
// Strict-CSP interactive filter and inspector for docs/blueprint.html.

(function () {
  "use strict";

  var filterButtons = Array.prototype.slice.call(
    document.querySelectorAll(".filter-btn[data-filter]")
  );
  var searchInput = document.getElementById("decl-search");
  var rows = Array.prototype.slice.call(
    document.querySelectorAll("tr[data-lean-decl]")
  );
  var statusBox = document.getElementById("filter-status");
  var detailBox = document.getElementById("decl-detail");
  var activeFilter = "all";

  function applyFilters() {
    var query = searchInput ? searchInput.value.trim().toLowerCase() : "";
    var visible = 0;
    rows.forEach(function (row) {
      var cat = row.getAttribute("data-category") || "";
      var txt = (row.textContent || "").toLowerCase();
      var catOk = activeFilter === "all" || cat === activeFilter;
      var queryOk = !query || txt.indexOf(query) !== -1;
      if (catOk && queryOk) {
        row.classList.remove("hidden-row");
        visible += 1;
      } else {
        row.classList.add("hidden-row");
      }
    });
    if (statusBox) {
      statusBox.textContent =
        "Showing " + visible + " of " + rows.length + " audited Lean 4.35.0 declarations.";
    }
  }

  function selectRow(row) {
    rows.forEach(function (r) {
      r.classList.remove("is-selected");
    });
    row.classList.add("is-selected");
    if (!detailBox) return;
    var decl = row.getAttribute("data-lean-decl") || "";
    var mod = row.getAttribute("data-module") || "";
    var cat = row.getAttribute("data-category-label") || "";
    var src = row.getAttribute("data-source") || "";
    var summary = row.getAttribute("data-summary") || "";
    detailBox.textContent =
      decl +
      " [" +
      cat +
      " | " +
      mod +
      "] -> " +
      summary +
      " (Reference: " +
      src +
      ")";
  }

  filterButtons.forEach(function (btn) {
    btn.addEventListener("click", function () {
      activeFilter = btn.getAttribute("data-filter") || "all";
      filterButtons.forEach(function (b) {
        b.setAttribute("aria-pressed", b === btn ? "true" : "false");
      });
      applyFilters();
    });
  });

  if (searchInput) {
    searchInput.addEventListener("input", applyFilters);
  }

  rows.forEach(function (row) {
    row.addEventListener("click", function () {
      selectRow(row);
    });
    row.addEventListener("keydown", function (ev) {
      if (ev.key === "Enter" || ev.key === " ") {
        ev.preventDefault();
        selectRow(row);
      }
    });
  });

  applyFilters();
})();
