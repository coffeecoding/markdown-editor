# Native Markdown

A small **Windows** editor with *CommonMark* parsing, Unicode (Grüße, Ελληνικά, 日本語, 😀), and `inline code`.

## Lists and quotes

- Open a document in View mode.
- Switch to Edit with **Ctrl+1**.
  - Your source and undo history stay in the editor.
- Return to View with **Ctrl+2** or toggle with **Ctrl+Tab**.

3. This list starts at three.
4. Save with **Ctrl+S**.

> A quoted paragraph with **strong text**.
>
> A second quoted paragraph.

---

```cpp
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    return 0;
}
```

[CommonMark](https://commonmark.org/) — the visible URL can be copied or Ctrl+clicked.

Hard break here.  
Next line. Named entities: &amp; &copy; &NotEqualTilde; &#x1F600;.

![A descriptive image fallback](example.png)

<div>HTML is shown as inert source, including its text.</div>
