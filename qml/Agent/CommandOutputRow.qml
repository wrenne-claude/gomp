pragma ComponentBehavior: Bound
import QtQuick
import Gomp

// Output of a slash command that ran inside omp without the model.
CodeBlock {
    required property string text
    code: text
    language: ""
    maxHeight: 360
}
