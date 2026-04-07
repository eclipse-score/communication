How To Document
===============
This section provides brief overview on how to document using Breathe directive and includes API documentation automatically generated from C++ source code.

.. note::
   API documentation is generated using Doxygen and integrated via Breathe.
   Due to Sphinx version < 9.x limitations with complex C++ templates, use specific class
   and function directives rather than namespace-level documentation.

Automatic API Documentation with @api Tag
------------------------------------------

The project includes an automated RST generation utility that extracts API items
tagged with ``@api`` in Doxygen comments and generates categorized documentation.

**How to use @api tag:**

Add the ``@api`` tag to any Doxygen comment block to include it in the generated
API documentation:

.. code-block:: cpp

   /**
    * @brief Connection handler for client communication
    * @api
    */
   class ClientConnection {
       // ...
   };

   /**
    * @brief Initialize the communication system
    * @return true if successful
    * @api
    */
   bool initialize();

**Generated documentation structure:**

The utility automatically creates:

* **API Index** - Overview of all tagged items
* **Namespace Reference** - Namespaces containing @api items
* **Class Reference** - Classes and structs with @api tag
* **Member Reference** - Functions and methods with @api tag

**Build integration:**

.. code-block:: starlark

   generate_api_rst(
       name = "generate_api_rst",
       doxygen_xml = "//path/to:doxygen_target",
       output_dir = "generated",
       project_name = "mw::com",
   )

The generated RST files are automatically included in the Sphinx documentation
under the "Generated API Documentation" section.

Using Breathe Directives
-------------------------

To document specific classes or functions, use these Breathe directives:

**Document a specific class:**

.. code-block:: rst

   .. doxygenclass:: score::message_passing::detail::ClientConnection
       :members:
       :protected-members:
       :undoc-members:

**Document a specific function:**

.. code-block:: rst

   .. doxygenfunction:: score::mw::com::functionName

**Document a struct:**

.. code-block:: rst

   .. doxygenstruct:: score::mw::com::ConfigStruct
       :members:

Example: ClientConnection
--------------------------

.. doxygenclass:: score::message_passing::detail::ClientConnection
    :members:
    :protected-members:
    :undoc-members:

Additional Resources
--------------------

For complete API documentation:

* Browse the Doxygen-generated HTML in ``bazel-bin/score/mw/com/design/doxygen_build/html/index.html``
* Review header files in ``score/mw/com/`` for inline documentation
* Check the design documentation at ``score/mw/com/design/``
