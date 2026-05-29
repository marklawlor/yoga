/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

import type Emitter from './emitters/Emitter.ts';
import type {ParsedStyles, ValueWithUnit} from './types.ts';

const INVISIBLE_BORDER_STYLES = new Set(['none', 'initial']);

function isMathExpression(value: string): boolean {
  const t = value.trimStart().toLowerCase();
  return (
    t.startsWith('calc(') ||
    t.startsWith('min(') ||
    t.startsWith('max(') ||
    t.startsWith('clamp(')
  );
}

function normalizeMathExpression(value: string): string {
  // Yoga math expressions are unitless (points). px is the only length unit the
  // native parser understands, so strip it (case-insensitively, including
  // leading-dot decimals like `.5px`). Any other unit (em, rem, vh, vw, …)
  // cannot be resolved statically and would make the native parser reject the
  // whole expression at runtime, silently clearing the property — so the
  // generated test would assert against an unconstrained node. Fail loudly here
  // instead so a bad fixture cannot produce a misleading passing test.
  const stripped = value.replace(/(\d*\.?\d+)px\b/gi, '$1');
  const badUnit = stripped.match(/\d*\.?\d+\s*([a-zA-Z]+)/);
  if (badUnit != null) {
    throw new Error(
      `Unsupported unit "${badUnit[1]}" in math expression "${value}". ` +
        'Yoga math expressions are unitless (points); only px and % are supported.',
    );
  }
  return stripped;
}

/**
 * Parse a raw inline style attribute string into a Map of property → value.
 * Expands common shorthands used in fixtures so individual longhand
 * properties are always present in the returned map.
 */
export function parseStyleAttribute(styleAttr: string): ParsedStyles {
  const styles: ParsedStyles = new Map();
  if (!styleAttr) return styles;

  const declarations = styleAttr.split(';');
  for (const decl of declarations) {
    const trimmed = decl.trim();
    if (!trimmed) continue;

    const colonIdx = trimmed.indexOf(':');
    if (colonIdx === -1) continue;

    const prop = trimmed.slice(0, colonIdx).trim();
    let value = trimmed.slice(colonIdx + 1).trim();

    // Strip single or double quotes from values (e.g., align-items: 'stretch')
    if (
      (value.startsWith("'") && value.endsWith("'")) ||
      (value.startsWith('"') && value.endsWith('"'))
    ) {
      value = value.slice(1, -1);
    }

    expandShorthand(styles, prop, value);
  }

  return styles;
}

function expandShorthand(
  styles: ParsedStyles,
  prop: string,
  value: string,
): void {
  switch (prop) {
    case 'flex': {
      // Only handle `flex: N` (single numeric value) as used in fixtures
      const n = parseFloat(value);
      if (!isNaN(n) && value.trim() === String(n)) {
        styles.set('flex-grow', String(n));
        styles.set('flex-shrink', '1');
        styles.set('flex-basis', '0%');
      } else {
        // Pass through complex flex values as-is
        styles.set(prop, value);
      }
      break;
    }

    default:
      styles.set(prop, value);
      break;
  }
}

/**
 * Map parsed CSS properties to Yoga emitter method calls.
 */
export function applyStyles(
  emitter: Emitter,
  nodeName: string,
  styles: ParsedStyles,
  isRoot: boolean,
): void {
  // Root nodes get implicit absolute positioning from the template CSS.
  if (isRoot && !styles.has('position')) {
    emitter.setPositionType(nodeName, positionValue('absolute'));
  }

  for (const [prop, value] of styles) {
    switch (prop) {
      case 'direction':
        emitter.setDirection(nodeName, directionValue(value));
        break;

      case 'position':
        if (value !== 'relative') {
          emitter.setPositionType(nodeName, positionValue(value));
        }
        break;

      case 'flex-direction':
        if (value !== 'column') {
          emitter.setFlexDirection(nodeName, flexDirectionValue(value));
        }
        break;

      case 'flex-wrap':
        if (value !== 'nowrap') {
          emitter.setFlexWrap(nodeName, wrapValue(value));
        }
        break;

      case 'justify-content': {
        const mapped = justifyValue(value);
        if (mapped !== undefined && value !== 'flex-start') {
          emitter.setJustifyContent(nodeName, mapped);
        }
        break;
      }

      case 'align-content': {
        const mapped = alignValue(value);
        if (mapped !== undefined && value !== 'flex-start') {
          emitter.setAlignContent(nodeName, mapped);
        }
        break;
      }

      case 'align-items': {
        const mapped = alignValue(value);
        if (mapped !== undefined && value !== 'stretch') {
          emitter.setAlignItems(nodeName, mapped);
        }
        break;
      }

      case 'align-self': {
        const mapped = alignValue(value);
        if (mapped !== undefined) {
          emitter.setAlignSelf(nodeName, mapped);
        }
        break;
      }

      case 'overflow':
        if (value !== 'visible') {
          emitter.setOverflow(nodeName, overflowValue(value));
        }
        break;

      case 'display':
        if (value !== 'flex') {
          emitter.setDisplay(nodeName, displayValue(value));
        }
        break;

      case 'box-sizing':
        if (value !== 'border-box') {
          emitter.setBoxSizing(nodeName, boxSizingValue(value));
        }
        break;

      case 'flex-grow':
        if (value !== '0') {
          emitter.setFlexGrow(nodeName, value);
        }
        break;

      case 'flex-shrink':
        if (value !== '0') {
          emitter.setFlexShrink(nodeName, value);
        }
        break;

      case 'flex-basis': {
        if (isMathExpression(value)) {
          emitter.setFlexBasisExpression(
            nodeName,
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null) emitter.setFlexBasis(nodeName, parsed);
        }
        break;
      }

      case 'aspect-ratio':
        emitter.setAspectRatio(nodeName, parseValueWithUnit(value));
        break;

      // Dimensions
      case 'width': {
        if (isMathExpression(value)) {
          emitter.setWidthExpression(nodeName, normalizeMathExpression(value));
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null) emitter.setWidth(nodeName, parsed);
        }
        break;
      }
      case 'height': {
        if (isMathExpression(value)) {
          emitter.setHeightExpression(nodeName, normalizeMathExpression(value));
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null) emitter.setHeight(nodeName, parsed);
        }
        break;
      }
      case 'min-width': {
        if (isMathExpression(value)) {
          emitter.setMinWidthExpression(
            nodeName,
            normalizeMathExpression(value),
          );
        } else if (value !== '0' && value !== '0px' && value !== 'auto') {
          const parsed = parseCssLength(value);
          if (parsed != null) emitter.setMinWidth(nodeName, parsed);
        }
        break;
      }
      case 'min-height': {
        if (isMathExpression(value)) {
          emitter.setMinHeightExpression(
            nodeName,
            normalizeMathExpression(value),
          );
        } else if (value !== '0' && value !== '0px' && value !== 'auto') {
          const parsed = parseCssLength(value);
          if (parsed != null) emitter.setMinHeight(nodeName, parsed);
        }
        break;
      }
      case 'max-width': {
        if (isMathExpression(value)) {
          emitter.setMaxWidthExpression(
            nodeName,
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null) emitter.setMaxWidth(nodeName, parsed);
        }
        break;
      }
      case 'max-height': {
        if (isMathExpression(value)) {
          emitter.setMaxHeightExpression(
            nodeName,
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null) emitter.setMaxHeight(nodeName, parsed);
        }
        break;
      }

      // Insets - physical
      case 'left': {
        if (isMathExpression(value)) {
          emitter.setPositionExpression(
            nodeName,
            edgeValue('left'),
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPosition(nodeName, edgeValue('left'), parsed);
        }
        break;
      }
      case 'top': {
        if (isMathExpression(value)) {
          emitter.setPositionExpression(
            nodeName,
            edgeValue('top'),
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPosition(nodeName, edgeValue('top'), parsed);
        }
        break;
      }
      case 'right': {
        if (isMathExpression(value)) {
          emitter.setPositionExpression(
            nodeName,
            edgeValue('right'),
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPosition(nodeName, edgeValue('right'), parsed);
        }
        break;
      }
      case 'bottom': {
        if (isMathExpression(value)) {
          emitter.setPositionExpression(
            nodeName,
            edgeValue('bottom'),
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPosition(nodeName, edgeValue('bottom'), parsed);
        }
        break;
      }

      // Insets - logical
      case 'inset-inline-start': {
        if (isMathExpression(value)) {
          emitter.setPositionExpression(
            nodeName,
            edgeValue('start'),
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPosition(nodeName, edgeValue('start'), parsed);
        }
        break;
      }
      case 'inset-inline-end': {
        if (isMathExpression(value)) {
          emitter.setPositionExpression(
            nodeName,
            edgeValue('end'),
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPosition(nodeName, edgeValue('end'), parsed);
        }
        break;
      }
      case 'inset-block-start': {
        if (isMathExpression(value)) {
          emitter.setPositionExpression(
            nodeName,
            edgeValue('top'),
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPosition(nodeName, edgeValue('top'), parsed);
        }
        break;
      }
      case 'inset-block-end': {
        if (isMathExpression(value)) {
          emitter.setPositionExpression(
            nodeName,
            edgeValue('bottom'),
            normalizeMathExpression(value),
          );
        } else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPosition(nodeName, edgeValue('bottom'), parsed);
        }
        break;
      }

      // Box shorthands
      case 'margin':
      case 'padding':
      case 'border-width': {
        if (prop !== 'border-width' && isMathExpression(value)) {
          const expr = normalizeMathExpression(value);
          if (prop === 'margin')
            emitter.setMarginExpression(nodeName, edgeValue('all'), expr);
          else emitter.setPaddingExpression(nodeName, edgeValue('all'), expr);
          break;
        }
        const parse =
          prop === 'border-width' ? parseBorderWidth : parseCssLength;
        const emit = (edge: string, v: ValueWithUnit) => {
          if (prop === 'margin') emitter.setMargin(nodeName, edge, v);
          else if (prop === 'padding') emitter.setPadding(nodeName, edge, v);
          else emitter.setBorder(nodeName, edge, v);
        };
        const parts = value.split(/\s+/);
        switch (parts.length) {
          case 1: {
            const parsed = parse(parts[0]);
            if (parsed != null) {
              emit(edgeValue('all'), parsed);
            }
            break;
          }
          case 2: {
            const vertVal = parse(parts[0]);
            const horizVal = parse(parts[1]);
            if (vertVal != null) {
              emit(edgeValue('vertical'), vertVal);
            }
            if (horizVal != null) {
              emit(edgeValue('horizontal'), horizVal);
            }
            break;
          }
          case 3: {
            const topVal = parse(parts[0]);
            const horizVal = parse(parts[1]);
            const bottomVal = parse(parts[2]);
            if (topVal != null) {
              emit(edgeValue('top'), topVal);
            }
            if (horizVal != null) {
              emit(edgeValue('horizontal'), horizVal);
            }
            if (bottomVal != null) {
              emit(edgeValue('bottom'), bottomVal);
            }
            break;
          }
          case 4: {
            const topVal = parse(parts[0]);
            const rightVal = parse(parts[1]);
            const bottomVal = parse(parts[2]);
            const leftVal = parse(parts[3]);
            if (topVal != null) {
              emit(edgeValue('top'), topVal);
            }
            if (rightVal != null) {
              emit(edgeValue('right'), rightVal);
            }
            if (bottomVal != null) {
              emit(edgeValue('bottom'), bottomVal);
            }
            if (leftVal != null) {
              emit(edgeValue('left'), leftVal);
            }
            break;
          }
        }
        break;
      }

      // Gap shorthand
      case 'gap': {
        if (isMathExpression(value)) {
          emitter.setGapExpression(
            nodeName,
            gutterValue('all'),
            normalizeMathExpression(value),
          );
          break;
        }
        const parts = value.split(/\s+/);
        if (parts.length === 1) {
          const parsed = parseCssLength(parts[0]);
          if (parsed != null) {
            emitter.setGap(nodeName, gutterValue('all'), parsed);
          }
        } else {
          const rowVal = parseCssLength(parts[0]);
          const colVal = parseCssLength(parts[1]);
          if (rowVal != null) {
            emitter.setGap(nodeName, gutterValue('row'), rowVal);
          }
          if (colVal != null) {
            emitter.setGap(nodeName, gutterValue('column'), colVal);
          }
        }
        break;
      }

      // Margins - shorthand logical
      case 'margin-inline': {
        const parts = value.split(/\s+/);
        if (parts.length === 1) {
          const parsed = parseCssLength(parts[0]);
          if (parsed != null) {
            emitter.setMargin(nodeName, edgeValue('horizontal'), parsed);
          }
        } else {
          const startVal = parseCssLength(parts[0]);
          const endVal = parseCssLength(parts[1]);
          if (startVal != null) {
            emitter.setMargin(nodeName, edgeValue('start'), startVal);
          }
          if (endVal != null) {
            emitter.setMargin(nodeName, edgeValue('end'), endVal);
          }
        }
        break;
      }
      case 'margin-block': {
        const parts = value.split(/\s+/);
        if (parts.length === 1) {
          const parsed = parseCssLength(parts[0]);
          if (parsed != null) {
            emitter.setMargin(nodeName, edgeValue('vertical'), parsed);
          }
        } else {
          const topVal = parseCssLength(parts[0]);
          const bottomVal = parseCssLength(parts[1]);
          if (topVal != null) {
            emitter.setMargin(nodeName, edgeValue('top'), topVal);
          }
          if (bottomVal != null) {
            emitter.setMargin(nodeName, edgeValue('bottom'), bottomVal);
          }
        }
        break;
      }

      // Margins - physical
      case 'margin-left': {
        if (isMathExpression(value))
          emitter.setMarginExpression(
            nodeName,
            edgeValue('left'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setMargin(nodeName, edgeValue('left'), parsed);
        }
        break;
      }
      case 'margin-top': {
        if (isMathExpression(value))
          emitter.setMarginExpression(
            nodeName,
            edgeValue('top'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setMargin(nodeName, edgeValue('top'), parsed);
        }
        break;
      }
      case 'margin-right': {
        if (isMathExpression(value))
          emitter.setMarginExpression(
            nodeName,
            edgeValue('right'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setMargin(nodeName, edgeValue('right'), parsed);
        }
        break;
      }
      case 'margin-bottom': {
        if (isMathExpression(value))
          emitter.setMarginExpression(
            nodeName,
            edgeValue('bottom'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setMargin(nodeName, edgeValue('bottom'), parsed);
        }
        break;
      }

      // Margins - logical
      case 'margin-inline-start': {
        if (isMathExpression(value))
          emitter.setMarginExpression(
            nodeName,
            edgeValue('start'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setMargin(nodeName, edgeValue('start'), parsed);
        }
        break;
      }
      case 'margin-inline-end': {
        if (isMathExpression(value))
          emitter.setMarginExpression(
            nodeName,
            edgeValue('end'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setMargin(nodeName, edgeValue('end'), parsed);
        }
        break;
      }
      case 'margin-block-start': {
        if (isMathExpression(value))
          emitter.setMarginExpression(
            nodeName,
            edgeValue('top'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setMargin(nodeName, edgeValue('top'), parsed);
        }
        break;
      }
      case 'margin-block-end': {
        if (isMathExpression(value))
          emitter.setMarginExpression(
            nodeName,
            edgeValue('bottom'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setMargin(nodeName, edgeValue('bottom'), parsed);
        }
        break;
      }

      // Padding - physical
      case 'padding-left': {
        if (isMathExpression(value))
          emitter.setPaddingExpression(
            nodeName,
            edgeValue('left'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPadding(nodeName, edgeValue('left'), parsed);
        }
        break;
      }
      case 'padding-top': {
        if (isMathExpression(value))
          emitter.setPaddingExpression(
            nodeName,
            edgeValue('top'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPadding(nodeName, edgeValue('top'), parsed);
        }
        break;
      }
      case 'padding-right': {
        if (isMathExpression(value))
          emitter.setPaddingExpression(
            nodeName,
            edgeValue('right'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPadding(nodeName, edgeValue('right'), parsed);
        }
        break;
      }
      case 'padding-bottom': {
        if (isMathExpression(value))
          emitter.setPaddingExpression(
            nodeName,
            edgeValue('bottom'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPadding(nodeName, edgeValue('bottom'), parsed);
        }
        break;
      }

      // Padding - logical
      case 'padding-inline-start': {
        if (isMathExpression(value))
          emitter.setPaddingExpression(
            nodeName,
            edgeValue('start'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPadding(nodeName, edgeValue('start'), parsed);
        }
        break;
      }
      case 'padding-inline-end': {
        if (isMathExpression(value))
          emitter.setPaddingExpression(
            nodeName,
            edgeValue('end'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPadding(nodeName, edgeValue('end'), parsed);
        }
        break;
      }
      case 'padding-inline': {
        const parts = value.split(/\s+/);
        if (parts.length === 1) {
          const parsed = parseCssLength(parts[0]);
          if (parsed != null) {
            emitter.setPadding(nodeName, edgeValue('horizontal'), parsed);
          }
        } else {
          const startVal = parseCssLength(parts[0]);
          const endVal = parseCssLength(parts[1]);
          if (startVal != null) {
            emitter.setPadding(nodeName, edgeValue('start'), startVal);
          }
          if (endVal != null) {
            emitter.setPadding(nodeName, edgeValue('end'), endVal);
          }
        }
        break;
      }
      case 'padding-block': {
        const parts = value.split(/\s+/);
        if (parts.length === 1) {
          const parsed = parseCssLength(parts[0]);
          if (parsed != null) {
            emitter.setPadding(nodeName, edgeValue('vertical'), parsed);
          }
        } else {
          const topVal = parseCssLength(parts[0]);
          const bottomVal = parseCssLength(parts[1]);
          if (topVal != null) {
            emitter.setPadding(nodeName, edgeValue('top'), topVal);
          }
          if (bottomVal != null) {
            emitter.setPadding(nodeName, edgeValue('bottom'), bottomVal);
          }
        }
        break;
      }
      case 'padding-block-start': {
        if (isMathExpression(value))
          emitter.setPaddingExpression(
            nodeName,
            edgeValue('top'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPadding(nodeName, edgeValue('top'), parsed);
        }
        break;
      }
      case 'padding-block-end': {
        if (isMathExpression(value))
          emitter.setPaddingExpression(
            nodeName,
            edgeValue('bottom'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setPadding(nodeName, edgeValue('bottom'), parsed);
        }
        break;
      }

      // Border - compound shorthands (e.g. `border: 10px solid black`)
      case 'border': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('all'), width);
        }
        break;
      }
      case 'border-top': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('top'), width);
        }
        break;
      }
      case 'border-right': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('right'), width);
        }
        break;
      }
      case 'border-bottom': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('bottom'), width);
        }
        break;
      }
      case 'border-left': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('left'), width);
        }
        break;
      }
      case 'border-block': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('vertical'), width);
        }
        break;
      }
      case 'border-inline': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('horizontal'), width);
        }
        break;
      }
      case 'border-block-start': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('top'), width);
        }
        break;
      }
      case 'border-block-end': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('bottom'), width);
        }
        break;
      }
      case 'border-inline-start': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('start'), width);
        }
        break;
      }
      case 'border-inline-end': {
        const width = extractBorderWidth(value);
        if (width != null) {
          emitter.setBorder(nodeName, edgeValue('end'), width);
        }
        break;
      }

      // Border - physical
      case 'border-left-width': {
        const borderStyle = styles.get('border-left-style');
        if (!borderStyle || !INVISIBLE_BORDER_STYLES.has(borderStyle)) {
          const parsed = parseBorderWidth(value);
          if (parsed != null) {
            emitter.setBorder(nodeName, edgeValue('left'), parsed);
          }
        }
        break;
      }
      case 'border-top-width': {
        const borderStyle = styles.get('border-top-style');
        if (!borderStyle || !INVISIBLE_BORDER_STYLES.has(borderStyle)) {
          const parsed = parseBorderWidth(value);
          if (parsed != null) {
            emitter.setBorder(nodeName, edgeValue('top'), parsed);
          }
        }
        break;
      }
      case 'border-right-width': {
        const borderStyle = styles.get('border-right-style');
        if (!borderStyle || !INVISIBLE_BORDER_STYLES.has(borderStyle)) {
          const parsed = parseBorderWidth(value);
          if (parsed != null) {
            emitter.setBorder(nodeName, edgeValue('right'), parsed);
          }
        }
        break;
      }
      case 'border-bottom-width': {
        const borderStyle = styles.get('border-bottom-style');
        if (!borderStyle || !INVISIBLE_BORDER_STYLES.has(borderStyle)) {
          const parsed = parseBorderWidth(value);
          if (parsed != null) {
            emitter.setBorder(nodeName, edgeValue('bottom'), parsed);
          }
        }
        break;
      }

      // Border - logical
      case 'border-inline-start-width': {
        const borderStyle = styles.get('border-inline-start-style');
        if (!borderStyle || !INVISIBLE_BORDER_STYLES.has(borderStyle)) {
          const parsed = parseBorderWidth(value);
          if (parsed != null) {
            emitter.setBorder(nodeName, edgeValue('start'), parsed);
          }
        }
        break;
      }
      case 'border-inline-end-width': {
        const borderStyle = styles.get('border-inline-end-style');
        if (!borderStyle || !INVISIBLE_BORDER_STYLES.has(borderStyle)) {
          const parsed = parseBorderWidth(value);
          if (parsed != null) {
            emitter.setBorder(nodeName, edgeValue('end'), parsed);
          }
        }
        break;
      }
      case 'border-block-start-width': {
        const borderStyle = styles.get('border-block-start-style');
        if (!borderStyle || !INVISIBLE_BORDER_STYLES.has(borderStyle)) {
          const parsed = parseBorderWidth(value);
          if (parsed != null) {
            emitter.setBorder(nodeName, edgeValue('top'), parsed);
          }
        }
        break;
      }
      case 'border-block-end-width': {
        const borderStyle = styles.get('border-block-end-style');
        if (!borderStyle || !INVISIBLE_BORDER_STYLES.has(borderStyle)) {
          const parsed = parseBorderWidth(value);
          if (parsed != null) {
            emitter.setBorder(nodeName, edgeValue('bottom'), parsed);
          }
        }
        break;
      }

      // Gap
      case 'row-gap': {
        if (isMathExpression(value))
          emitter.setGapExpression(
            nodeName,
            gutterValue('row'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setGap(nodeName, gutterValue('row'), parsed);
        }
        break;
      }
      case 'column-gap': {
        if (isMathExpression(value))
          emitter.setGapExpression(
            nodeName,
            gutterValue('column'),
            normalizeMathExpression(value),
          );
        else {
          const parsed = parseCssLength(value);
          if (parsed != null)
            emitter.setGap(nodeName, gutterValue('column'), parsed);
        }
        break;
      }
    }
  }
}

// Value parsers

export function parseValueWithUnit(value: string): ValueWithUnit {
  if (value === 'auto') return {type: 'auto'};
  if (value === 'undefined') return {type: 'undefined'};
  if (value === 'max-content') return {type: 'max-content'};
  if (value === 'fit-content') return {type: 'fit-content'};
  if (value === 'stretch' || value === '-webkit-fill-available')
    return {type: 'stretch'};
  if (value.endsWith('%')) return {type: 'percent', value: parseFloat(value)};
  return {type: 'points', value: parseFloat(value)};
}

/**
 * Parse a CSS length value, rejecting unitless non-zero numbers.
 * In CSS, only `0` is valid without a unit for length properties.
 * Invalid values (e.g. `padding-top: 10` without `px`) return null.
 */
export function parseCssLength(value: string): ValueWithUnit | null {
  if (value === 'auto') return {type: 'auto'};
  if (value === 'undefined') return {type: 'undefined'};
  if (value === 'max-content') return {type: 'max-content'};
  if (value === 'fit-content') return {type: 'fit-content'};
  if (value === 'stretch' || value === '-webkit-fill-available')
    return {type: 'stretch'};
  if (value.endsWith('%')) return {type: 'percent', value: parseFloat(value)};
  if (value.endsWith('px')) return {type: 'points', value: parseFloat(value)};
  // Unitless 0 is valid
  const num = parseFloat(value);
  if (num === 0 && !isNaN(num)) return {type: 'points', value: 0};
  // Unitless non-zero number is invalid CSS for length properties
  return null;
}

/**
 * Parse a CSS border-width value. Rejects unitless non-zero numbers
 * and percentage values (CSS border-width does not accept percentages).
 */
function parseBorderWidth(value: string): ValueWithUnit | null {
  const parsed = parseCssLength(value);
  if (parsed != null && parsed.type === 'percent') return null;
  return parsed;
}

/**
 * Extract the width component from a compound border shorthand value
 * like `10px solid black` or `solid black 5px`. Returns null if the
 * border style is invisible (none/initial) or no valid width is found.
 */
function extractBorderWidth(value: string): ValueWithUnit | null {
  const parts = value.split(/\s+/);
  const BORDER_STYLES = new Set([
    'none',
    'hidden',
    'dotted',
    'dashed',
    'solid',
    'double',
    'groove',
    'ridge',
    'inset',
    'outset',
    'initial',
  ]);

  let widthStr: string | null = null;
  let styleStr: string | null = null;

  for (const part of parts) {
    if (BORDER_STYLES.has(part)) {
      styleStr = part;
    } else if (/^[\d.]/.test(part)) {
      widthStr = part;
    }
    // color values are ignored
  }

  if (styleStr && INVISIBLE_BORDER_STYLES.has(styleStr)) return null;
  if (widthStr == null) return null;
  return parseBorderWidth(widthStr);
}

// Enum value mappers

function directionValue(value: string): string {
  const map: Record<string, string> = {
    ltr: 'YGDirectionLTR',
    rtl: 'YGDirectionRTL',
    inherit: 'YGDirectionInherit',
  };
  return map[value] ?? value;
}

function flexDirectionValue(value: string): string {
  const map: Record<string, string> = {
    row: 'YGFlexDirectionRow',
    'row-reverse': 'YGFlexDirectionRowReverse',
    column: 'YGFlexDirectionColumn',
    'column-reverse': 'YGFlexDirectionColumnReverse',
  };
  return map[value] ?? value;
}

function justifyValue(value: string): string | undefined {
  const map: Record<string, string> = {
    center: 'YGJustifyCenter',
    'space-around': 'YGJustifySpaceAround',
    'space-between': 'YGJustifySpaceBetween',
    'space-evenly': 'YGJustifySpaceEvenly',
    'flex-start': 'YGJustifyFlexStart',
    'flex-end': 'YGJustifyFlexEnd',
    stretch: 'YGJustifyStretch',
    start: 'YGJustifyStart',
    end: 'YGJustifyEnd',
    auto: 'YGJustifyAuto',
  };
  return map[value];
}

function alignValue(value: string): string | undefined {
  const map: Record<string, string> = {
    auto: 'YGAlignAuto',
    center: 'YGAlignCenter',
    stretch: 'YGAlignStretch',
    'flex-start': 'YGAlignFlexStart',
    'flex-end': 'YGAlignFlexEnd',
    'space-between': 'YGAlignSpaceBetween',
    'space-around': 'YGAlignSpaceAround',
    'space-evenly': 'YGAlignSpaceEvenly',
    baseline: 'YGAlignBaseline',
    start: 'YGAlignStart',
    end: 'YGAlignEnd',
  };
  return map[value];
}

function positionValue(value: string): string {
  const map: Record<string, string> = {
    absolute: 'YGPositionTypeAbsolute',
    static: 'YGPositionTypeStatic',
    relative: 'YGPositionTypeRelative',
  };
  return map[value] ?? 'YGPositionTypeRelative';
}

function wrapValue(value: string): string {
  const map: Record<string, string> = {
    wrap: 'YGWrapWrap',
    'wrap-reverse': 'YGWrapWrapReverse',
    nowrap: 'YGWrapNoWrap',
  };
  return map[value] ?? value;
}

function overflowValue(value: string): string {
  const map: Record<string, string> = {
    visible: 'YGOverflowVisible',
    hidden: 'YGOverflowHidden',
    scroll: 'YGOverflowScroll',
  };
  return map[value] ?? value;
}

function displayValue(value: string): string {
  const map: Record<string, string> = {
    flex: 'YGDisplayFlex',
    none: 'YGDisplayNone',
    contents: 'YGDisplayContents',
  };
  return map[value] ?? value;
}

function boxSizingValue(value: string): string {
  const map: Record<string, string> = {
    'border-box': 'YGBoxSizingBorderBox',
    'content-box': 'YGBoxSizingContentBox',
  };
  return map[value] ?? value;
}

function edgeValue(edge: string): string {
  const map: Record<string, string> = {
    left: 'YGEdgeLeft',
    right: 'YGEdgeRight',
    top: 'YGEdgeTop',
    bottom: 'YGEdgeBottom',
    start: 'YGEdgeStart',
    end: 'YGEdgeEnd',
    all: 'YGEdgeAll',
    vertical: 'YGEdgeVertical',
    horizontal: 'YGEdgeHorizontal',
  };
  return map[edge] ?? edge;
}

function gutterValue(gutter: string): string {
  const map: Record<string, string> = {
    row: 'YGGutterRow',
    column: 'YGGutterColumn',
    all: 'YGGutterAll',
  };
  return map[gutter] ?? gutter;
}

export function getFlexDirectionForMeasure(styles: ParsedStyles): string {
  const fd = styles.get('flex-direction');
  return flexDirectionValue(fd ?? 'column');
}
