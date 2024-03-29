possible new editing features:

- @! comment
    - a mechanism for inserting comments that are not propagated to tangled
      outputs (but might be propagated to woven outputs)

- @+'includes':<cmath>
    - a mechanism for appending a single line to an existing chunk, or defining
      a one liner chunk

- @++
    - a mechanism for appending to the most recently mentioned chunk

- nested chunk definitions
    - so I could append to other chunks in the middle of a chunk definition

- limited support for invoking chunks multiple times, specifically and solely
  intended for copyright and license boilerplate, or better, a sanctioned workflow
  for limited text template / macro expansion, e.g. based on a limited yaml
  frontmatter, possibly as a separate tool

- ability to make directories if they don't already exist

- output a cachefile enabling better incremental build support and to clean up
  tangled files that have changed name

pseudo-weaving:

- copy the input to the output, but replace control sequences with templates
  whose fields are interpolated based on the values associated with the control
  sequence
    - so an invocation gets turned into a hyperlinked ref
    - a definition has the control sequences stripped, a header added, and
      gets a list of its own invocations
    - add a control sequence just for repeating a previous definition in the
      woven output (the reader is reminded that blah blah was defined as...)

untangling:

- embed lili activation codes into the tangled output and allow the machine
  source to be untangled and changes pushed into the literate source, allowing
  edits to be made directly to the machine source (benefitting from the
  extensive tooling available for editing such documents) and then sync'd back
  to the literate source
    - will require the structure of the literate source to be represented in
      code so that the untangled machine source can be woven back in
    - will require the ATSIGN, append sequence, beginning and end of chunks
      to be embedded textually into machine source??? Can we somehow get
      away without making the machine source horribly ugly?
    - will require machine-language specific knowledge, mostly how to write
      comments.
    - complicated!

probably will not implement:

- allow input from stdin
    - there hasn't yet been a reason to implement this, although it's possible
      to imagine a workflow where a file is first processed by something other
      than lili, and then piped into lili. Maybe one day if there's need.

will not implement:

- allow multiple input files
    - this introduces a lot of complexity surrounding scope; it's necessary to
      ensure that a chunk defined in a.lili can't be invoked from b.lili. It's
      also completely unnecessary since it's trivially easy to simply invoke
      lili more than once
