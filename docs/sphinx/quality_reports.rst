Quality Reports
===============

Nightly quality job results are built and published to GitHub Pages after each
nightly run of the `Nightly Quality Jobs`_ workflow.

.. list-table::
   :widths: 20 45 35
   :header-rows: 1

   * - Job
     - Description
     - Report
   * - Coverage
     - Line and branch coverage from C++ unit tests (gcov/lcov)
     - `Coverage report <quality/coverage/index.html>`_
   * - CodeQL
     - Static security and quality analysis
     - Not yet configured
   * - Clang-Tidy
     - C++ static analysis
     - Not yet configured

`Open Quality Dashboard <quality/index.html>`_

.. note::

   Quality reports are updated every night.  If a link returns a 404, the
   first nightly run has not yet completed.  Check the `Nightly Quality Jobs`_
   workflow page for the current run status.

.. _Nightly Quality Jobs: https://github.com/ahmed0mousa/communication/actions/workflows/nightly_quality.yml
