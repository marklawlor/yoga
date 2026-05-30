/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

import {
  Align,
  BoxSizing,
  Direction,
  Display,
  Edge,
  FlexDirection,
  Gutter,
  Justify,
  Overflow,
  PositionType,
  Wrap,
  Node as YogaNode,
} from 'yoga-layout';

export type AlignContent =
  | 'flex-start'
  | 'flex-end'
  | 'center'
  | 'stretch'
  | 'space-between'
  | 'space-around'
  | 'space-evenly';

export type AlignItems =
  | 'flex-start'
  | 'flex-end'
  | 'center'
  | 'stretch'
  | 'baseline';

export type JustifyContent =
  | 'flex-start'
  | 'flex-end'
  | 'center'
  | 'space-between'
  | 'space-around'
  | 'space-evenly';

export type FlexStyle = {
  alignContent?: AlignContent;
  alignItems?: AlignItems;
  alignSelf?: AlignItems;
  aspectRatio?: number;
  borderBottomWidth?: number;
  borderEndWidth?: number;
  borderLeftWidth?: number;
  borderRightWidth?: number;
  borderStartWidth?: number;
  borderTopWidth?: number;
  borderWidth?: number;
  borderInlineWidth?: number;
  borderBlockWidth?: number;
  bottom?: number | `${number}%` | string;
  boxSizing?: 'border-box' | 'content-box';
  direction?: 'ltr' | 'rtl';
  display?: 'none' | 'flex' | 'contents';
  end?: number | `${number}%` | string;
  flex?: number;
  flexBasis?: number | 'auto' | `${number}%` | string;
  flexDirection?: 'row' | 'column' | 'row-reverse' | 'column-reverse';
  rowGap?: number | `${number}%` | string;
  gap?: number | `${number}%` | string;
  columnGap?: number | `${number}%` | string;
  flexGrow?: number;
  flexShrink?: number;
  flexWrap?: 'wrap' | 'nowrap' | 'wrap-reverse';
  height?: number | 'auto' | `${number}%` | string;
  justifyContent?: JustifyContent;
  left?: number | `${number}%` | string;
  margin?: number | 'auto' | `${number}%` | string;
  marginBottom?: number | 'auto' | `${number}%` | string;
  marginEnd?: number | 'auto' | `${number}%` | string;
  marginLeft?: number | 'auto' | `${number}%` | string;
  marginRight?: number | 'auto' | `${number}%` | string;
  marginStart?: number | 'auto' | `${number}%` | string;
  marginTop?: number | 'auto' | `${number}%` | string;
  marginInline?: number | 'auto' | `${number}%` | string;
  marginBlock?: number | 'auto' | `${number}%` | string;
  maxHeight?: number | `${number}%` | string;
  maxWidth?: number | `${number}%` | string;
  minHeight?: number | `${number}%` | string;
  minWidth?: number | `${number}%` | string;
  overflow?: 'visible' | 'hidden' | 'scroll';
  padding?: number | `${number}%` | string;
  paddingBottom?: number | `${number}%` | string;
  paddingEnd?: number | `${number}%` | string;
  paddingLeft?: number | `${number}%` | string;
  paddingRight?: number | `${number}%` | string;
  paddingStart?: number | `${number}%` | string;
  paddingTop?: number | `${number}%` | string;
  paddingInline?: number | `${number}%` | string;
  paddingBlock?: number | `${number}%` | string;
  position?: 'absolute' | 'relative' | 'static';
  right?: number | `${number}%` | string;
  start?: number | `${number}%` | string;
  top?: number | `${number}%` | string;
  insetInline?: number | `${number}%` | string;
  insetBlock?: number | `${number}%` | string;
  inset?: number | `${number}%` | string;
  width?: number | 'auto' | `${number}%` | string;
};

function isCssMathExpression(value: string): boolean {
  return (
    value.startsWith('calc(') ||
    value.startsWith('env(') ||
    value.startsWith('min(') ||
    value.startsWith('max(') ||
    value.startsWith('clamp(')
  );
}

export function applyStyle(node: YogaNode, style: FlexStyle = {}): void {
  for (const key of Object.keys(style)) {
    try {
      switch (key) {
        case 'alignContent':
          node.setAlignContent(alignContent(style.alignContent));
          break;
        case 'alignItems':
          node.setAlignItems(alignItems(style.alignItems));
          break;
        case 'alignSelf':
          node.setAlignSelf(alignItems(style.alignSelf));
          break;
        case 'aspectRatio':
          node.setAspectRatio(style.aspectRatio);
          break;
        case 'borderBottomWidth':
          node.setBorder(Edge.Bottom, style.borderBottomWidth);
          break;
        case 'borderEndWidth':
          node.setBorder(Edge.End, style.borderEndWidth);
          break;
        case 'borderLeftWidth':
          node.setBorder(Edge.Left, style.borderLeftWidth);
          break;
        case 'borderRightWidth':
          node.setBorder(Edge.Right, style.borderRightWidth);
          break;
        case 'borderStartWidth':
          node.setBorder(Edge.Start, style.borderStartWidth);
          break;
        case 'borderTopWidth':
          node.setBorder(Edge.Top, style.borderTopWidth);
          break;
        case 'borderWidth':
          node.setBorder(Edge.All, style.borderWidth);
          break;
        case 'borderInlineWidth':
          node.setBorder(Edge.Horizontal, style.borderInlineWidth);
          break;
        case 'borderBlockWidth':
          node.setBorder(Edge.Vertical, style.borderBlockWidth);
          break;
        case 'bottom':
          if (
            typeof style.bottom === 'string' &&
            isCssMathExpression(style.bottom)
          ) {
            node.setPositionExpression(Edge.Bottom, style.bottom);
          } else {
            node.setPosition(
              Edge.Bottom,
              style.bottom as number | `${number}%`,
            );
          }
          break;
        case 'boxSizing':
          node.setBoxSizing(boxSizing(style.boxSizing));
          break;
        case 'direction':
          node.setDirection(direction(style.direction));
          break;
        case 'display':
          node.setDisplay(display(style.display));
          break;
        case 'end':
          if (typeof style.end === 'string' && isCssMathExpression(style.end)) {
            node.setPositionExpression(Edge.End, style.end);
          } else {
            node.setPosition(Edge.End, style.end as number | `${number}%`);
          }
          break;
        case 'flex':
          node.setFlex(style.flex);
          break;
        case 'flexBasis':
          if (
            typeof style.flexBasis === 'string' &&
            isCssMathExpression(style.flexBasis)
          ) {
            node.setFlexBasisExpression(style.flexBasis);
          } else {
            node.setFlexBasis(
              style.flexBasis as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'flexDirection':
          node.setFlexDirection(flexDirection(style.flexDirection));
          break;
        case 'rowGap':
          if (
            typeof style.rowGap === 'string' &&
            isCssMathExpression(style.rowGap)
          ) {
            node.setGapExpression(Gutter.Row, style.rowGap);
          } else {
            node.setGap(Gutter.Row, style.rowGap as number);
          }
          break;
        case 'gap':
          if (typeof style.gap === 'string' && isCssMathExpression(style.gap)) {
            node.setGapExpression(Gutter.All, style.gap);
          } else {
            node.setGap(Gutter.All, style.gap as number);
          }
          break;
        case 'columnGap':
          if (
            typeof style.columnGap === 'string' &&
            isCssMathExpression(style.columnGap)
          ) {
            node.setGapExpression(Gutter.Column, style.columnGap);
          } else {
            node.setGap(Gutter.Column, style.columnGap as number);
          }
          break;
        case 'flexGrow':
          node.setFlexGrow(style.flexGrow);
          break;
        case 'flexShrink':
          node.setFlexShrink(style.flexShrink);
          break;
        case 'flexWrap':
          node.setFlexWrap(flexWrap(style.flexWrap));
          break;
        case 'height':
          if (
            typeof style.height === 'string' &&
            isCssMathExpression(style.height)
          ) {
            node.setHeightExpression(style.height);
          } else {
            node.setHeight(style.height as number | 'auto' | `${number}%`);
          }
          break;
        case 'justifyContent':
          node.setJustifyContent(justifyContent(style.justifyContent));
          break;
        case 'left':
          if (
            typeof style.left === 'string' &&
            isCssMathExpression(style.left)
          ) {
            node.setPositionExpression(Edge.Left, style.left);
          } else {
            node.setPosition(Edge.Left, style.left as number | `${number}%`);
          }
          break;
        case 'margin':
          if (
            typeof style.margin === 'string' &&
            isCssMathExpression(style.margin)
          ) {
            node.setMarginExpression(Edge.All, style.margin);
          } else {
            node.setMargin(
              Edge.All,
              style.margin as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'marginBottom':
          if (
            typeof style.marginBottom === 'string' &&
            isCssMathExpression(style.marginBottom)
          ) {
            node.setMarginExpression(Edge.Bottom, style.marginBottom);
          } else {
            node.setMargin(
              Edge.Bottom,
              style.marginBottom as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'marginEnd':
          if (
            typeof style.marginEnd === 'string' &&
            isCssMathExpression(style.marginEnd)
          ) {
            node.setMarginExpression(Edge.End, style.marginEnd);
          } else {
            node.setMargin(
              Edge.End,
              style.marginEnd as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'marginLeft':
          if (
            typeof style.marginLeft === 'string' &&
            isCssMathExpression(style.marginLeft)
          ) {
            node.setMarginExpression(Edge.Left, style.marginLeft);
          } else {
            node.setMargin(
              Edge.Left,
              style.marginLeft as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'marginRight':
          if (
            typeof style.marginRight === 'string' &&
            isCssMathExpression(style.marginRight)
          ) {
            node.setMarginExpression(Edge.Right, style.marginRight);
          } else {
            node.setMargin(
              Edge.Right,
              style.marginRight as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'marginStart':
          if (
            typeof style.marginStart === 'string' &&
            isCssMathExpression(style.marginStart)
          ) {
            node.setMarginExpression(Edge.Start, style.marginStart);
          } else {
            node.setMargin(
              Edge.Start,
              style.marginStart as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'marginTop':
          if (
            typeof style.marginTop === 'string' &&
            isCssMathExpression(style.marginTop)
          ) {
            node.setMarginExpression(Edge.Top, style.marginTop);
          } else {
            node.setMargin(
              Edge.Top,
              style.marginTop as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'marginInline':
          if (
            typeof style.marginInline === 'string' &&
            isCssMathExpression(style.marginInline)
          ) {
            node.setMarginExpression(Edge.Horizontal, style.marginInline);
          } else {
            node.setMargin(
              Edge.Horizontal,
              style.marginInline as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'marginBlock':
          if (
            typeof style.marginBlock === 'string' &&
            isCssMathExpression(style.marginBlock)
          ) {
            node.setMarginExpression(Edge.Vertical, style.marginBlock);
          } else {
            node.setMargin(
              Edge.Vertical,
              style.marginBlock as number | 'auto' | `${number}%`,
            );
          }
          break;
        case 'maxHeight':
          if (
            typeof style.maxHeight === 'string' &&
            isCssMathExpression(style.maxHeight)
          ) {
            node.setMaxHeightExpression(style.maxHeight);
          } else {
            node.setMaxHeight(style.maxHeight as number | `${number}%`);
          }
          break;
        case 'maxWidth':
          if (
            typeof style.maxWidth === 'string' &&
            isCssMathExpression(style.maxWidth)
          ) {
            node.setMaxWidthExpression(style.maxWidth);
          } else {
            node.setMaxWidth(style.maxWidth as number | `${number}%`);
          }
          break;
        case 'minHeight':
          if (
            typeof style.minHeight === 'string' &&
            isCssMathExpression(style.minHeight)
          ) {
            node.setMinHeightExpression(style.minHeight);
          } else {
            node.setMinHeight(style.minHeight as number | `${number}%`);
          }
          break;
        case 'minWidth':
          if (
            typeof style.minWidth === 'string' &&
            isCssMathExpression(style.minWidth)
          ) {
            node.setMinWidthExpression(style.minWidth);
          } else {
            node.setMinWidth(style.minWidth as number | `${number}%`);
          }
          break;
        case 'overflow':
          node.setOverflow(overflow(style.overflow));
          break;
        case 'padding':
          if (
            typeof style.padding === 'string' &&
            isCssMathExpression(style.padding)
          ) {
            node.setPaddingExpression(Edge.All, style.padding);
          } else {
            node.setPadding(Edge.All, style.padding as number | `${number}%`);
          }
          break;
        case 'paddingBottom':
          if (
            typeof style.paddingBottom === 'string' &&
            isCssMathExpression(style.paddingBottom)
          ) {
            node.setPaddingExpression(Edge.Bottom, style.paddingBottom);
          } else {
            node.setPadding(
              Edge.Bottom,
              style.paddingBottom as number | `${number}%`,
            );
          }
          break;
        case 'paddingEnd':
          if (
            typeof style.paddingEnd === 'string' &&
            isCssMathExpression(style.paddingEnd)
          ) {
            node.setPaddingExpression(Edge.End, style.paddingEnd);
          } else {
            node.setPadding(
              Edge.End,
              style.paddingEnd as number | `${number}%`,
            );
          }
          break;
        case 'paddingLeft':
          if (
            typeof style.paddingLeft === 'string' &&
            isCssMathExpression(style.paddingLeft)
          ) {
            node.setPaddingExpression(Edge.Left, style.paddingLeft);
          } else {
            node.setPadding(
              Edge.Left,
              style.paddingLeft as number | `${number}%`,
            );
          }
          break;
        case 'paddingRight':
          if (
            typeof style.paddingRight === 'string' &&
            isCssMathExpression(style.paddingRight)
          ) {
            node.setPaddingExpression(Edge.Right, style.paddingRight);
          } else {
            node.setPadding(
              Edge.Right,
              style.paddingRight as number | `${number}%`,
            );
          }
          break;
        case 'paddingStart':
          if (
            typeof style.paddingStart === 'string' &&
            isCssMathExpression(style.paddingStart)
          ) {
            node.setPaddingExpression(Edge.Start, style.paddingStart);
          } else {
            node.setPadding(
              Edge.Start,
              style.paddingStart as number | `${number}%`,
            );
          }
          break;
        case 'paddingTop':
          if (
            typeof style.paddingTop === 'string' &&
            isCssMathExpression(style.paddingTop)
          ) {
            node.setPaddingExpression(Edge.Top, style.paddingTop);
          } else {
            node.setPadding(
              Edge.Top,
              style.paddingTop as number | `${number}%`,
            );
          }
          break;
        case 'paddingInline':
          if (
            typeof style.paddingInline === 'string' &&
            isCssMathExpression(style.paddingInline)
          ) {
            node.setPaddingExpression(Edge.Horizontal, style.paddingInline);
          } else {
            node.setPadding(
              Edge.Horizontal,
              style.paddingInline as number | `${number}%`,
            );
          }
          break;
        case 'paddingBlock':
          if (
            typeof style.paddingBlock === 'string' &&
            isCssMathExpression(style.paddingBlock)
          ) {
            node.setPaddingExpression(Edge.Vertical, style.paddingBlock);
          } else {
            node.setPadding(
              Edge.Vertical,
              style.paddingBlock as number | `${number}%`,
            );
          }
          break;
        case 'position':
          node.setPositionType(position(style.position));
          break;
        case 'right':
          if (
            typeof style.right === 'string' &&
            isCssMathExpression(style.right)
          ) {
            node.setPositionExpression(Edge.Right, style.right);
          } else {
            node.setPosition(Edge.Right, style.right as number | `${number}%`);
          }
          break;
        case 'start':
          if (
            typeof style.start === 'string' &&
            isCssMathExpression(style.start)
          ) {
            node.setPositionExpression(Edge.Start, style.start);
          } else {
            node.setPosition(Edge.Start, style.start as number | `${number}%`);
          }
          break;
        case 'top':
          if (typeof style.top === 'string' && isCssMathExpression(style.top)) {
            node.setPositionExpression(Edge.Top, style.top);
          } else {
            node.setPosition(Edge.Top, style.top as number | `${number}%`);
          }
          break;
        case 'insetInline':
          if (
            typeof style.insetInline === 'string' &&
            isCssMathExpression(style.insetInline)
          ) {
            node.setPositionExpression(Edge.Horizontal, style.insetInline);
          } else {
            node.setPosition(
              Edge.Horizontal,
              style.insetInline as number | `${number}%`,
            );
          }
          break;
        case 'insetBlock':
          if (
            typeof style.insetBlock === 'string' &&
            isCssMathExpression(style.insetBlock)
          ) {
            node.setPositionExpression(Edge.Vertical, style.insetBlock);
          } else {
            node.setPosition(
              Edge.Vertical,
              style.insetBlock as number | `${number}%`,
            );
          }
          break;
        case 'inset':
          if (
            typeof style.inset === 'string' &&
            isCssMathExpression(style.inset)
          ) {
            node.setPositionExpression(Edge.All, style.inset);
          } else {
            node.setPosition(Edge.All, style.inset as number | `${number}%`);
          }
          break;
        case 'width':
          if (
            typeof style.width === 'string' &&
            isCssMathExpression(style.width)
          ) {
            node.setWidthExpression(style.width);
          } else {
            node.setWidth(style.width as number | 'auto' | `${number}%`);
          }
          break;
      }
    } catch (e) {
      // Fail gracefully
    }
  }
}

function alignContent(str?: AlignContent): Align {
  switch (str) {
    case 'flex-start':
      return Align.FlexStart;
    case 'flex-end':
      return Align.FlexEnd;
    case 'center':
      return Align.Center;
    case 'stretch':
      return Align.Stretch;
    case 'space-between':
      return Align.SpaceBetween;
    case 'space-around':
      return Align.SpaceAround;
    case 'space-evenly':
      return Align.SpaceEvenly;
  }
  throw new Error(`"${str}" is not a valid value for alignContent`);
}

function alignItems(str?: AlignItems): Align {
  switch (str) {
    case 'flex-start':
      return Align.FlexStart;
    case 'flex-end':
      return Align.FlexEnd;
    case 'center':
      return Align.Center;
    case 'stretch':
      return Align.Stretch;
    case 'baseline':
      return Align.Baseline;
  }
  throw new Error(`"${str}" is not a valid value for alignItems`);
}

function boxSizing(str?: 'border-box' | 'content-box'): BoxSizing {
  switch (str) {
    case 'border-box':
      return BoxSizing.BorderBox;
    case 'content-box':
      return BoxSizing.ContentBox;
  }
  throw new Error(`"${str}" is not a valid value for boxSizing`);
}

function direction(str?: 'ltr' | 'rtl'): Direction {
  switch (str) {
    case 'ltr':
      return Direction.LTR;
    case 'rtl':
      return Direction.RTL;
  }
  throw new Error(`"${str}" is not a valid value for direction`);
}

function display(str?: 'none' | 'flex' | 'contents'): Display {
  switch (str) {
    case 'none':
      return Display.None;
    case 'flex':
      return Display.Flex;
    case 'contents':
      return Display.Contents;
  }
  throw new Error(`"${str}" is not a valid value for display`);
}

function flexDirection(
  str?: 'row' | 'column' | 'row-reverse' | 'column-reverse',
): FlexDirection {
  switch (str) {
    case 'row':
      return FlexDirection.Row;
    case 'column':
      return FlexDirection.Column;
    case 'row-reverse':
      return FlexDirection.RowReverse;
    case 'column-reverse':
      return FlexDirection.ColumnReverse;
  }
  throw new Error(`"${str}" is not a valid value for flexDirection`);
}

function flexWrap(str?: 'wrap' | 'nowrap' | 'wrap-reverse'): Wrap {
  switch (str) {
    case 'wrap':
      return Wrap.Wrap;
    case 'nowrap':
      return Wrap.NoWrap;
    case 'wrap-reverse':
      return Wrap.WrapReverse;
  }
  throw new Error(`"${str}" is not a valid value for flexWrap`);
}

function justifyContent(str?: JustifyContent): Justify {
  switch (str) {
    case 'flex-start':
      return Justify.FlexStart;
    case 'flex-end':
      return Justify.FlexEnd;
    case 'center':
      return Justify.Center;
    case 'space-between':
      return Justify.SpaceBetween;
    case 'space-around':
      return Justify.SpaceAround;
    case 'space-evenly':
      return Justify.SpaceEvenly;
  }
  throw new Error(`"${str}" is not a valid value for justifyContent`);
}

function overflow(str?: 'visible' | 'hidden' | 'scroll'): Overflow {
  switch (str) {
    case 'visible':
      return Overflow.Visible;
    case 'hidden':
      return Overflow.Hidden;
    case 'scroll':
      return Overflow.Scroll;
  }
  throw new Error(`"${str}" is not a valid value for overflow`);
}

function position(str?: 'absolute' | 'relative' | 'static'): PositionType {
  switch (str) {
    case 'absolute':
      return PositionType.Absolute;
    case 'relative':
      return PositionType.Relative;
    case 'static':
      return PositionType.Static;
  }
  throw new Error(`"${str}" is not a valid value for position`);
}
