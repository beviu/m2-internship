#import "./simple-arrow.typ": simple-arrow

#let stmt(body, base-color: none, bold: false) = {
  let fill = if bold {
    base-color.lighten(60%)
  } else {
    base-color.lighten(75%).desaturate(25%)
  }

  pad(
    y: .2em,
    rect(
      width: 10em,
      fill: fill,
      stroke: black,
      inset: (x: .4em, y: .5em),
      align(center, body),
    ),
  )
}

#let kernel-stmt = stmt.with(base-color: red)
#let user-stmt = stmt.with(base-color: blue)
#let internal-stmt = stmt.with(base-color: green)

#let get-location(callback) = {
  let label = label("loc-tracker")

  [
    #metadata(none) // A label must be attached to some content.
    #label
  ]

  context {
    let location = query(selector(label).before(here())).last().location()
    callback(location)
  }
}

#let execution(braces: (), ..threads) = context {
  let threads = threads.pos()

  let header(thread) = {
    let (name, .._statements) = thread
    strong(name)
  }

  let column(thread) = {
    let (_name, ..statements) = thread
    let elements = statements.map(statement => {
      if type(statement) == content {
        statement
      } else if (
        type(statement) == dictionary
          and statement.keys().contains("wait-for-event")
      ) {
        let event = statement.at("wait-for-event")
        get-location(location => {
          let current-position = location.position()
          let event-position = query(selector(label("execution-" + event)))
            .last()
            .location()
            .position()
          let y = current-position.y
          if y < event-position.y {
            block(height: event-position.y - y, spacing: 0pt)
            y = event-position.y
          }
          align(
            left,
            block(
              place(
                simple-arrow(
                  start: (
                    event-position.x - current-position.x,
                    event-position.y - y,
                  ),
                  end: (0pt, 0pt),
                  thickness: .1em,
                ),
              ),
              spacing: 0pt,
              inset: .2em,
            ),
          )
        })
      } else if (
        type(statement) == dictionary and statement.keys().contains("set-event")
      ) {
        let event = statement.at("set-event")
        [
          #metadata(none) // A label must be attached to some content.
          #label("execution-" + event)
        ]
      } else {
        panic(
          "statement is neither content, nor a dictionary with a \"set-event\" or \"wait-for-event\" key:",
          statement,
        )
      }
    })
    stack(dir: ttb, ..elements)
  }

  let main-table = table(
    columns: threads.len(),
    stroke: (x, y) => if threads.len() > 0 and x < threads.len() - 1 {
      (right: (dash: "dotted"))
    },
    table.header(..threads.map(header)),
    ..threads.map(column),
  )

  let brace(args) = {
    let (start-event, end-event, description) = args
    get-location(location => {
      let current-position = location.position()
      let start-position = query(selector(label("execution-" + start-event)))
        .last()
        .location()
        .position()
      let end-position = query(selector(label("execution-" + end-event)))
        .last()
        .location()
        .position()
      move(
        grid(
          columns: (auto, auto),
          column-gutter: .5em,
          align: horizon,
          $ lr(#block(height: end-position.y - start-position.y) }) $,
          align(left, description),
        ),
        dy: start-position.y - current-position.y,
      )
    })
  }

  stack(dir: ltr, main-table, braces.map(brace).join())
}
