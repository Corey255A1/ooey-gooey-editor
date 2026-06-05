# Part 6: Advanced Editor UX, Cursor Preservation, and Squiggle Diagnostics

Building a visual designer exposes subtle but critical user experience (UX) challenges. When a user types code, they expect the visual canvas to update instantly without freezing the app. Conversely, when the user edits a property inside the Property Grid, the text editor must update without resetting the caret cursor position or scroll state. Furthermore, when syntax or linter errors occur, the editor must provide visual diagnostics in the form of squiggly lines and error warnings.

This post details the implementation of these high-fidelity UX mechanics, focusing on asynchronous parsing, cursor preservation, text squiggle rendering, and toolkit primitives.

---

## 1. Zero-Latency Dynamic Updates & Debouncing

In a split-view editor, parsing the DSL on every keypress can cause performance issues and UI freezes, especially for large layouts. If the compilation or layout solver blocks the main thread, typing will lag.

To solve this, we implement two main optimizations:
1. **Debouncing**: We do not parse the DSL immediately on keypress. Instead, we reset a timer (e.g., 200ms). The parser only runs when the user stops typing for the duration of the timeout.
2. **Error Recovery**: If the parser encounters a syntax error during typing, we do not throw exceptions or crash. Instead, we catch the error, highlight the line containing the issue, and leave the visual canvas in its last known valid state.

```
 [ Keystroke ] ──► [ Reset Debounce Timer (200ms) ]
                         │
                         ▼ (Timer Expires)
               [ Parse DSL (tooey) ]
               /                   \
        (Success)                 (Syntax Error)
           │                             │
           ▼                             ▼
   [ Update Stage Canvas ]     [ Draw Error Squiggles ]
                               [ Keep Canvas Unchanged ]
```

---

## 2. Cursor and Viewport Scroll Preservation

A common bug in split-view editors is "cursor jumping." When a user modifies a property in the Property Grid, the view model serializes the AST, generating a new DSL string and setting it on the text editor's text property. 
If the text editor simply replaces its buffer, the caret (cursor) position resets to the top left `(0, 0)`, and the scrollbars scroll back to the top.

To maintain a fluid editing experience, the editor must perform **cursor preservation**:

1. **Capture State**: Before applying the serialized DSL string from the view model, capture the current state of the `RichTextBox`:
   ```cpp
   int caret_line = codeEditor->get_cursor_line();
   int caret_col = codeEditor->get_cursor_col();
   int scroll_x = codeEditor->get_scroll_x();
   int scroll_y = codeEditor->get_scroll_y();
   bool selection = codeEditor->has_selection();
   ```
2. **Apply Update**: Update the text content.
3. **Restore State**: Clamp and restore the cursor position, scroll offsets, and active selection ranges.

This ensure that round-trip synchronization between the visual canvas and the code editor remains completely seamless to the developer.

---

## 3. Visual Diagnostics: Rendering Text Squiggles

When the linter or compiler detects warnings or errors, the editor must draw squiggly lines under the offending text. 

We extended the `RichTextBox` component to support wavy squiggles by creating a custom vertex rendering pass.

```
Text:      VBox id=mainLayout spacing=20 width="MatchParent":
Squiggle:                              ~~~~~~~~~~ (Warning/Error)
```

### A. Wavy Line Geometry Generation
We construct a custom `Geometry` structure using `PrimitiveType::Lines` to generate a connected zigzag pattern under the text:

```cpp
static Geometry make_squiggle_geometry(int x1, int x2, int y, Color color) {
    Geometry geom;
    geom.type = PrimitiveType::Lines;
    
    int step = 2; // Pixel interval for wave peaks and valleys
    int amp = 1;  // Wave amplitude height
    
    int vertex_index = 0;
    float prev_x = static_cast<float>(x1);
    float prev_y = static_cast<float>(y);
    
    for (int x = x1 + 1; x <= x2; ++x) {
        float next_x = static_cast<float>(x);
        float next_y = static_cast<float>(y + (((x / step) % 2 == 0) ? amp : -amp));
        
        geom.vertices.push_back({ .x=prev_x, .y=prev_y, .color=color });
        geom.vertices.push_back({ .x=next_x, .y=next_y, .color=color });
        
        geom.indices.push_back(vertex_index++);
        geom.indices.push_back(vertex_index++);
        
        prev_x = next_x;
        prev_y = next_y;
    }
    return geom;
}
```

### B. Render Pass Integration
In the `RichTextBox::draw_content` draw pass, we query active linter errors and draw squiggles right under the text baseline:

```cpp
// Inside draw_content loop for line 'l'
for (const auto& sq : squiggles_) {
    if (sq.line_idx == l) {
        int sq_start = std::max(0, sq.start_col);
        int sq_end = std::min(static_cast<int>(lines_[l].size()), sq.end_col);
        if (sq_start < sq_end) {
            int x1 = content_view_->bounds().x + 8 + get_column_x_offset(l, sq_start);
            int x2 = content_view_->bounds().x + 8 + get_column_x_offset(l, sq_end);
            int y = current_y + char_h; // Place line under character baseline
            target.draw_geometry(make_squiggle_geometry(x1, x2, y, sq.color));
        }
    }
}
```

---

## 4. Toolkit Requirements for Modern IDE Controls

Building high-fidelity visual building environments places specific demands on the underlying C++ GUI framework. A framework must provide the following primitives:

1. **DPI-Aware Coordinate Translators**: Conversion helpers to translate window screen space pixels into logical UI layout coordinates.
2. **Text Metrics APIs**: Detailed horizontal character metrics (like `get_column_x_offset`) to place cursors, selections, and squiggles exactly between characters.
3. **Double-Buffered Draw Targets**: Double buffering to eliminate flickering during real-time layout updates.
4. **Scissor Clipping Stack**: Clipping hierarchies (`push_clip`/`pop_clip`) to prevent scrollable viewport children from rendering outside layout boundaries.

By satisfying these requirements, the OOEY framework enables developers to construct professional editor tools.
