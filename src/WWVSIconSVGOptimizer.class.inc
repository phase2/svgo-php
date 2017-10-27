<?php
/**
 * @file Optimizer class that mimics SVGO as closely as possible.
 *
 * This class acts as a utility class, meaning that it's initiated for use just
 * for its functionality and does not correspond to any one specific object
 * supplied to it. (See example usage below.)
 *
 * Example usage:
 *
 * $sanitizer = new WWVSIconSVGOptimizer();
 * $svg_file_contents = file_get_contents('/path/to/some/svg/file.svg');
 * $optimized_svg_file_contents = $sanitizer->optimize($svg_file_contents);
 *
 * Important websites for reference:
 *
 * SVGOMG - https://jakearchibald.github.io/svgomg/
 * The UI that WW editors use to optimize SVGs before upload and that we are
 * trying to replicate.
 *
 * SVGO - https://github.com/svg/svgo
 * The JS library that powers the SVGOMG UI and contains all of the logic that
 * we are trying to replicate.
 *
 * BTS2 SVG Configuration Reference -
 * https://confluence.tools.weightwatchers.com/display/VS/BTS2+SVG+icon+configuration#BTS2SVGiconconfiguration-CleanSVGs
 * The configuration that editors are required use on SVGOMG to optimize SVGs
 * and that we are aiming to replicate.
 *
 * Helpers:
 *
 * Regex Tester - https://regex101.com/
 * We're processing quite a few regular expressions throughout this library, and
 * this is a very helpful resource for double checking patterns that you need
 * confirmation on.
 *
 * Process notes:
 *
 *  - You can change the test image being processed by this script by changing
 *    the $test_svg_file variable in wwvs_icon_svg_preprocess_page() in the
 *    wwvs_icon_svg.module file.
 *  - Read through the plugins that exist in the SVGO library
 *    (https://github.com/svg/svgo/tree/master/plugins).
 *  - Use the example images supplied from the SVGO library in the SVGOMG tool,
 *    located in the test-svgs directory in the wwvs_icon_svg module directory
 *    and confirm the changes made by each plugin when it is turned on.
 *  - Use xdebug to pause (add a breakpoint) before and after processing of each
 *    plugin to review the changes made. (See $this->svg_file['content'] for the
 *    output SVG XML.) Make sure you place the ending breakpoint *after* the
 *    `$this->setXML($xml)` or `$this->setContent($svg)` to review the plugin's
 *    results.
 *
 * Additional notes:
 *
 *  - This class depends on the PHP Sanitizer class, which supplies the ability
 *    to whitelist tags and attributes for SVG cleanliness. This class maintains
 *    a list of tags and attributes to remove from those whitelists. Any other
 *    tags or attributes required to be removed must be done using a custom
 *    plugin method in this class.
 *  - The base class also provides the ability to minify the file and remove the
 *    default XML description tag, both of which we opted to do with this class.
 *    (See $this->setDefaultOptions().) However, the Sanitizer minify option
 *    does not remove empty text elements, which we are doing in this class.
 *    (See $this->removeEmptyText().)
 *
 */

libraries_load('svg-sanitizer');

module_load_include('inc', 'wwvs_icon_svg', 'includes/WWVSIconSVGOptimizerAttributes.class');
module_load_include('inc', 'wwvs_icon_svg', 'includes/WWVSIconSVGOptimizerTags.class');

class WWVSIconSVGOptimizer extends enshrined\svgSanitize\Sanitizer {

  /**
   * Default options.
   */
  const TAGS_TO_REMOVE = array('style', 'script', 'title', 'metadata', 'desc', 'image', 'img');
  const ATTRS_TO_REMOVE = array('version');

  /**
   * @var string The SVG file being sanitized.
   */
  private $svg_file;

  /**
   * @var string The default namespace for SVGs, overwritten if it's different
   * in the SVG file.
   */
  private $defaultNS = 'http://www.w3.org/2000/svg';

  public function __construct() {
    parent::__construct();

    $this->svg_file = array(
      'content' => '',
      'xml' => '',
    );
  }

  /**
   * Set default options for the optimizer before performing cleanup.
   */
  public function setDefaultOptions() {
    $this->minify(TRUE);
    $this->removeXMLTag(TRUE);

    $tags = new WWVSIconSVGOptimizerTags();
    $tags->removeTags(self::TAGS_TO_REMOVE);
    $this->setAllowedTags($tags);

    $attributes = new WWVSIconSVGOptimizerAttributes();
    $attributes->removeAttributes(self::ATTRS_TO_REMOVE);
    $this->setAllowedAttrs($attributes);

    // Get the default namespace and register it for xpath.
    preg_match('/xmlns="(.+?)"/i', $this->svg_file['content'], $matches);

    // Overwrite the default namespace with the document's namespace.
    if (isset($matches[1]) && $this->defaultNS != $matches[1]) {
      $this->defaultNS = $matches[1];
    }
  }

  /**
   * Clean the SVG using our settings.
   *
   * @param string $svg_file_contents The path to the SVG file being sanitized.
   */
  public function optimize($svg_file_contents) {
    if (empty($svg_file_contents)) {
      return FALSE;
    }

    $this->svg_file = array(
      'content' => $svg_file_contents,
      'xml' => new DOMDocument(),
    );

    $this->svg_file['xml']->loadXML($this->svg_file['content']);
    $this->svg_file['xml']->normalizeDocument();

    $this->setDefaultOptions();

    $this->setContent(parent::sanitize($this->svg_file['content']));

    /**
     * The plugins that are commented out below are either being refactored or
     * are not complete.
     */

    $this->removeEmptyText();
    $this->cleanupIDs();
    $this->removeUnusedDefs();
    $this->removeEditorData();
    $this->removeUnusedNamespaces();
    $this->removeViewBox();
    $this->convertStyleToAttrs();
    $this->minifyColors();
    $this->cleanupListOfValues();
    $this->cleanupNumericValues();
    $this->cleanupEnableBackground();
    $this->removeEmptyAttributes();
    $this->removeHiddenElements();
//    $this->removeUnknownsAndDefaults();
//    $this->removeUselessStrokeAndFill();
//    $this->removeUnneededGroupAttrs();
//    $this->convertPathData();
//    $this->mergePaths();
    $this->collapseUselessGroups();
    $this->cleanupTransforms();
//    $this->removeEmptyContainers();
    $this->sortAttributes();
    $this->addAriaHidden();

    return $this->svg_file['content'];
  }

  /**
   * Plugin functions & required properties.
   */

  /**
   * Specific editor namespaces to look for and remove, according to SVGO.
   *
   * @var array
   */
  private $editorNamespaces = array(
    'http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd',
    'http://inkscape.sourceforge.net/DTD/sodipodi-0.dtd',
    'http://www.inkscape.org/namespaces/inkscape',
    'http://www.bohemiancoding.com/sketch/ns',
    'http://ns.adobe.com/AdobeIllustrator/10.0/',
    'http://ns.adobe.com/Graphs/1.0/',
    'http://ns.adobe.com/AdobeSVGViewerExtensions/3.0/',
    'http://ns.adobe.com/Variables/1.0/',
    'http://ns.adobe.com/SaveForWeb/1.0/',
    'http://ns.adobe.com/Extensibility/1.0/',
    'http://ns.adobe.com/Flows/1.0/',
    'http://ns.adobe.com/ImageReplacement/1.0/',
    'http://ns.adobe.com/GenericCustomNamespace/1.0/',
    'http://ns.adobe.com/XPath/1.0/',
  );

  /**
   * Attributes to convert from the style attribute value string to separate
   * HTML attributes.
   *
   * @var array
   */
  private $styleAttrs = array(
    'alignment-baseline',
    'baseline-shift',
    'buffered-rendering',
    'clip',
    'clip-path',
    'clip-rule',
    'color',
    'color-interpolation',
    'color-interpolation-filters',
    'color-profile',
    'color-rendering',
    'cursor',
    'direction',
    'display',
    'dominant-baseline',
    'enable-background',
    'fill',
    'fill-opacity',
    'fill-rule',
    'filter',
    'flood-color',
    'flood-opacity',
    'font-family',
    'font-size',
    'font-size-adjust',
    'font-stretch',
    'font-style',
    'font-variant',
    'font-weight',
    'glyph-orientation-horizontal',
    'glyph-orientation-vertical',
    'image-rendering',
    'kerning',
    'letter-spacing',
    'lighting-color',
    'marker-end',
    'marker-mid',
    'marker-start',
    'mask',
    'opacity',
    'overflow',
    'pointer-events',
    'shape-rendering',
    'solid-color',
    'solid-opacity',
    'stop-color',
    'stop-opacity',
    'stroke',
    'stroke-dasharray',
    'stroke-dashoffset',
    'stroke-linecap',
    'stroke-linejoin',
    'stroke-miterlimit',
    'stroke-opacity',
    'stroke-width',
    'paint-order',
    'text-anchor',
    'text-decoration',
    'text-overflow',
    'white-space',
    'text-rendering',
    'unicode-bidi',
    'vector-effect',
    'viewport-fill',
    'viewport-fill-opacity',
    'visibility',
    'white-space',
    'word-spacing',
    'writing-mode',
  );

  /**
   * Attributes to look for and round numbers.
   *
   * @var array
   */
  private $numberAttrs = array(
    'points',
    'enable-background',
    'viewBox',
    'stroke-dasharray',
    'dx',
    'dy',
    'x',
    'y'
  );

  private $inheritableAttrs = array(
    'clip-rule',
    'color',
    'color-interpolation',
    'color-interpolation-filters',
    'color-profile',
    'color-rendering',
    'cursor',
    'direction',
    'fill',
    'fill-opacity',
    'fill-rule',
    'font',
    'font-family',
    'font-size',
    'font-size-adjust',
    'font-stretch',
    'font-style',
    'font-variant',
    'font-weight',
    'glyph-orientation-horizontal',
    'glyph-orientation-vertical',
    'image-rendering',
    'kerning',
    'letter-spacing',
    'marker',
    'marker-end',
    'marker-mid',
    'marker-start',
    'pointer-events',
    'shape-rendering',
    'stroke',
    'stroke-dasharray',
    'stroke-dashoffset',
    'stroke-linecap',
    'stroke-linejoin',
    'stroke-miterlimit',
    'stroke-opacity',
    'stroke-width',
    'text-anchor',
    'text-rendering',
    'transform',
    'visibility',
    'white-space',
    'word-spacing',
    'writing-mode',
  );

  private $elemsGroups = array(
    'animation' => array('animate', 'animateColor', 'animateMotion', 'animateTransform', 'set'),
    'descriptive' => array('desc', 'metadata', 'title'),
    'shape' => array('circle', 'ellipse', 'line', 'path', 'polygon', 'polyline', 'rect'),
    'structural' => array('defs', 'g', 'svg', 'symbol', 'use'),
    'paintServer' => array('solidColor', 'linearGradient', 'radialGradient', 'meshGradient', 'pattern', 'hatch'),
    'nonRendering' => array('linearGradient', 'radialGradient', 'pattern', 'clipPath', 'mask', 'marker', 'symbol', 'filter', 'solidColor'),
    'container' => array('a', 'defs', 'g', 'marker', 'mask', 'missing-glyph', 'pattern', 'svg', 'switch', 'symbol', 'foreignObject'),
    'textContent' => array('altGlyph', 'altGlyphDef', 'altGlyphItem', 'glyph', 'glyphRef', 'textPath', 'text', 'tref', 'tspan'),
    'textContentChild' => array('altGlyph', 'textPath', 'tref', 'tspan'),
    'lightSource' => array('feDiffuseLighting', 'feSpecularLighting', 'feDistantLight', 'fePointLight', 'feSpotLight'),
    'filterPrimitive' => array('feBlend', 'feColorMatrix', 'feComponentTransfer', 'feComposite', 'feConvolveMatrix', 'feDiffuseLighting', 'feDisplacementMap', 'feFlood', 'feGaussianBlur', 'feImage', 'feMerge', 'feMorphology', 'feOffset', 'feSpecularLighting', 'feTile', 'feTurbulence')
  );

  /**
   * Conversions from different units to pixels.
   *
   * @var array
   */
  private $absoluteLengths = array(
    'cm' => 96/2.54,
    'mm' => 96/25.4,
    'in' => 96,
    'pt' => 4/3,
    'pc' => 16,
  );

  /**
   * The order in which attributes should exist in the SVG markup.
   *
   * @var array
   */
  private $attrOrder = array(
    'id',
    'width',
    'height',
    'x',
    'x1',
    'x2',
    'y',
    'y1',
    'y2',
    'cx',
    'cy',
    'r',
    'fill',
    'stroke',
    'marker',
    'd',
    'points'
  );

  private $attrsGroupsDefaults = array(
    'core' => array( 'xml:space' => 'preserve' ),
    'filterPrimitive' => array( 'x' => '0', 'y' => '0', 'width' => '100%', 'height' => '100%' ),
    'presentation' => array(
      'clip' => 'auto',
      'clip-path' => 'none',
      'clip-rule' => 'nonzero',
      'mask' => 'none',
      'opacity' => '1',
      'solid-color' => '#000',
      'solid-opacity' => '1',
      'stop-color' => '#000',
      'stop-opacity' => '1',
      'fill-opacity' => '1',
      'fill-rule' => 'nonzero',
      'fill' => '#000',
      'stroke' => 'none',
      'stroke-width' => '1',
      'stroke-linecap' => 'butt',
      'stroke-linejoin' => 'miter',
      'stroke-miterlimit' => '4',
      'stroke-dasharray' => 'none',
      'stroke-dashoffset' => '0',
      'stroke-opacity' => '1',
      'paint-order' => 'normal',
      'vector-effect' => 'none',
      'viewport-fill' => 'none',
      'viewport-fill-opacity' => '1',
      'display' => 'inline',
      'visibility' => 'visible',
      'marker-start' => 'none',
      'marker-mid' => 'none',
      'marker-end' => 'none',
      'color-interpolation' => 'sRGB',
      'color-interpolation-filters' => 'linearRGB',
      'color-rendering' => 'auto',
      'shape-rendering' => 'auto',
      'text-rendering' => 'auto',
      'image-rendering' => 'auto',
      'buffered-rendering' => 'auto',
      'font-style' => 'normal',
      'font-variant' => 'normal',
      'font-weight' => 'normal',
      'font-stretch' => 'normal',
      'font-size' => 'medium',
      'font-size-adjust' => 'none',
      'kerning' => 'auto',
      'letter-spacing' => 'normal',
      'word-spacing' => 'normal',
      'text-decoration' => 'none',
      'text-anchor' => 'start',
      'text-overflow' => 'clip',
      'writing-mode' => 'lr-tb',
      'glyph-orientation-vertical' => 'auto',
      'glyph-orientation-horizontal' => '0deg',
      'direction' => 'ltr',
      'unicode-bidi' => 'normal',
      'dominant-baseline' => 'auto',
      'alignment-baseline' => 'baseline',
      'baseline-shift' => 'baseline'
    ),
    'transferFunction' => array( 'slope' => '1', 'intercept' => '0', 'amplitude' => '1', 'exponent' => '1', 'offset' => '0' )
  );

  private $colorsNames = array(
    'aliceblue' => '#f0f8ff',
    'antiquewhite' => '#faebd7',
    'aqua' => '#0ff',
    'aquamarine' => '#7fffd4',
    'azure' => '#f0ffff',
    'beige' => '#f5f5dc',
    'bisque' => '#ffe4c4',
    'black' => '#000',
    'blanchedalmond' => '#ffebcd',
    'blue' => '#00f',
    'blueviolet' => '#8a2be2',
    'brown' => '#a52a2a',
    'burlywood' => '#deb887',
    'cadetblue' => '#5f9ea0',
    'chartreuse' => '#7fff00',
    'chocolate' => '#d2691e',
    'coral' => '#ff7f50',
    'cornflowerblue' => '#6495ed',
    'cornsilk' => '#fff8dc',
    'crimson' => '#dc143c',
    'cyan' => '#0ff',
    'darkblue' => '#00008b',
    'darkcyan' => '#008b8b',
    'darkgoldenrod' => '#b8860b',
    'darkgray' => '#a9a9a9',
    'darkgreen' => '#006400',
    'darkkhaki' => '#bdb76b',
    'darkmagenta' => '#8b008b',
    'darkolivegreen' => '#556b2f',
    'darkorange' => '#ff8c00',
    'darkorchid' => '#9932cc',
    'darkred' => '#8b0000',
    'darksalmon' => '#e9967a',
    'darkseagreen' => '#8fbc8f',
    'darkslateblue' => '#483d8b',
    'darkslategray' => '#2f4f4f',
    'darkturquoise' => '#00ced1',
    'darkviolet' => '#9400d3',
    'deeppink' => '#ff1493',
    'deepskyblue' => '#00bfff',
    'dimgray' => '#696969',
    'dodgerblue' => '#1e90ff',
    'firebrick' => '#b22222',
    'floralwhite' => '#fffaf0',
    'forestgreen' => '#228b22',
    'fuchsia' => '#f0f',
    'gainsboro' => '#dcdcdc',
    'ghostwhite' => '#f8f8ff',
    'gold' => '#ffd700',
    'goldenrod' => '#daa520',
    'gray' => '#808080',
    'green' => '#008000',
    'greenyellow' => '#adff2f',
    'honeydew' => '#f0fff0',
    'hotpink' => '#ff69b4',
    'indianred' => '#cd5c5c',
    'indigo' => '#4b0082',
    'ivory' => '#fffff0',
    'khaki' => '#f0e68c',
    'lavender' => '#e6e6fa',
    'lavenderblush' => '#fff0f5',
    'lawngreen' => '#7cfc00',
    'lemonchiffon' => '#fffacd',
    'lightblue' => '#add8e6',
    'lightcoral' => '#f08080',
    'lightcyan' => '#e0ffff',
    'lightgoldenrodyellow' => '#fafad2',
    'lightgreen' => '#90ee90',
    'lightgrey' => '#d3d3d3',
    'lightpink' => '#ffb6c1',
    'lightsalmon' => '#ffa07a',
    'lightseagreen' => '#20b2aa',
    'lightskyblue' => '#87cefa',
    'lightslategray' => '#789',
    'lightsteelblue' => '#b0c4de',
    'lightyellow' => '#ffffe0',
    'lime' => '#0f0',
    'limegreen' => '#32cd32',
    'linen' => '#faf0e6',
    'magenta' => '#f0f',
    'maroon' => '#800000',
    'mediumaquamarine' => '#66cdaa',
    'mediumblue' => '#0000cd',
    'mediumorchid' => '#ba55d3',
    'mediumpurple' => '#9370db',
    'mediumseagreen' => '#3cb371',
    'mediumslateblue' => '#7b68ee',
    'mediumspringgreen' => '#00fa9a',
    'mediumturquoise' => '#48d1cc',
    'mediumvioletred' => '#c71585',
    'midnightblue' => '#191970',
    'mintcream' => '#f5fffa',
    'mistyrose' => '#ffe4e1',
    'moccasin' => '#ffe4b5',
    'navajowhite' => '#ffdead',
    'navy' => '#000080',
    'oldlace' => '#fdf5e6',
    'olive' => '#808000',
    'olivedrab' => '#6b8e23',
    'orange' => '#ffa500',
    'orangered' => '#ff4500',
    'orchid' => '#da70d6',
    'palegoldenrod' => '#eee8aa',
    'palegreen' => '#98fb98',
    'paleturquoise' => '#afeeee',
    'palevioletred' => '#db7093',
    'papayawhip' => '#ffefd5',
    'peachpuff' => '#ffdab9',
    'peru' => '#cd853f',
    'pink' => '#ffc0cb',
    'plum' => '#dda0dd',
    'powderblue' => '#b0e0e6',
    'purple' => '#800080',
    'red' => '#f00',
    'rosybrown' => '#bc8f8f',
    'royalblue' => '#4169e1',
    'saddlebrown' => '#8b4513',
    'salmon' => '#fa8072',
    'sandybrown' => '#f4a460',
    'seagreen' => '#2e8b57',
    'seashell' => '#fff5ee',
    'sienna' => '#a0522d',
    'silver' => '#c0c0c0',
    'skyblue' => '#87ceeb',
    'slateblue' => '#6a5acd',
    'slategray' => '#708090',
    'snow' => '#fffafa',
    'springgreen' => '#00ff7f',
    'steelblue' => '#4682b4',
    'tan' => '#d2b48c',
    'teal' => '#008080',
    'thistle' => '#d8bfd8',
    'tomato' => '#ff6347',
    'turquoise' => '#40e0d0',
    'violet' => '#ee82ee',
    'wheat' => '#f5deb3',
    'white' => '#fff',
    'whitesmoke' => '#f5f5f5',
    'yellow' => '#ff0',
    'yellowgreen' => '#9acd32',
  );

  private $colorsShortNames = array(
    '#f0ffff' => 'azure',
    '#f5f5dc' => 'beige',
    '#ffe4c4' => 'bisque',
    '#a52a2a' => 'brown',
    '#ff7f50' => 'coral',
    '#ffd700' => 'gold',
    '#808080' => 'gray',
    '#008000' => 'green',
    '#4b0082' => 'indigo',
    '#fffff0' => 'ivory',
    '#f0e68c' => 'khaki',
    '#faf0e6' => 'linen',
    '#800000' => 'maroon',
    '#000080' => 'navy',
    '#808000' => 'olive',
    '#ffa500' => 'orange',
    '#da70d6' => 'orchid',
    '#cd853f' => 'peru',
    '#ffc0cb' => 'pink',
    '#dda0dd' => 'plum',
    '#800080' => 'purple',
    '#f00' => 'red',
    '#fa8072' => 'salmon',
    '#a0522d' => 'sienna',
    '#c0c0c0' => 'silver',
    '#fffafa' => 'snow',
    '#d2b48c' => 'tan',
    '#008080' => 'teal',
    '#ff6347' => 'tomato',
    '#ee82ee' => 'violet',
    '#f5deb3' => 'wheat',
  );

  /**
   * Remove all empty #text nodes in the SVG file.
   */
  private function removeEmptyText() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    $empty_text_nodes = $xpath->query('//text()');
    foreach ($empty_text_nodes as $node) {
      $stop = '';
      if (!$node->hasChildNodes() && (empty($node->nodeValue) || strlen(preg_replace('/\s/', '', $node->nodeValue)) == 0)) {
        $node->parentNode->removeChild($node);
      }
    }

    $this->setXML($xml);
  }

  /**
   * Remove unused IDs and rename used IDs to simpler characters.
   *
   * TODO - Refactor to use xpath?
   */
  private function cleanupIDs() {
    $svg = $this->svg_file['content'];
    $generateIDchars = array(
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    );

    // Get all of the IDs and references.
    preg_match_all('/id=["|\'](.+?)["|\']/i', $svg, $all_ids);
    preg_match_all('/href=["|\']#(.+?)["|\']/i', $svg, $link_refs);
    preg_match_all('/url\(["|\']?#(.+?)["|\']?\)/i', $svg, $style_refs);

    if (!empty($all_ids[1])) {
      // Make sure we always start at the beginning.
      reset($generateIDchars);

      foreach ($all_ids[1] as $id) {
        $replacement = '';

        if (in_array($id, $link_refs[1]) || in_array($id, $style_refs[1])) {
          /**
           * This $id is referenced, so we should minify it to a single
           * character id.
           */
          $pattern = "/$id/i";
          $replacement = current($generateIDchars);
          next($generateIDchars);
        }
        else {
          /**
           * This $id is not referenced anywhere, so we should remove the id
           * attribute entirely (along with the preceding space) and leave
           * $replacement empty.
           */
          $pattern = "/ id=\"$id\"/i";
        }

        $svg = preg_replace($pattern, $replacement, $svg);
      }
    }

    $this->setContent($svg);
  }

  /**
   * Remove defs without IDs.
   */
  private function removeUnusedDefs() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    // Get all tags inside of <defs> that don't have IDs...
    $defs_without_ids = $xpath->query('/*/*[name() = \'defs\']/*[not(@id)]');

    if ($defs_without_ids->length > 0) {
      foreach ($defs_without_ids as $node) {
        $node->parentNode->removeChild($node);
      }
    }

    $this->setXML($xml);
  }

  /**
   * Remove editor namespaces.
   */
  private function removeEditorData() {
    $svg = $this->svg_file['content'];

    foreach ($this->editorNamespaces as $ns_url) {

      // Find and remove the namespace declarations in the svg tag.
      $ns_url_pattern = '/ xmlns:(\w+?)="' . str_replace('/', '\/', $ns_url) . '"/i';
      if (preg_match($ns_url_pattern, $svg, $matches)) {
        $svg = preg_replace($ns_url_pattern, '', $svg);

        // Find and remove all references to those namespaces.
        $ns = $matches[1];
        $svg = preg_replace('/ ' . $ns . ':\w+?=".+?"/', '', $svg);
      }
    }

    $this->setContent($svg);
  }

  /**
   * Remove any namespaces that aren't being referenced.
   */
  private function removeUnusedNamespaces() {
    $svg = $this->svg_file['content'];

    // Get remaining namespaces.
    if(preg_match_all('/xmlns:(\w+?)=".+?"/i', $svg, $matches)) {

      /**
       * If none of the remaining namespaces are being used anywhere, remove the
       * namespace from the <svg> tag.
       */
      foreach($matches[1] as $ns) {
        if (!preg_match('/ ' . $ns . ':\w+?=".+?"/i', $svg)) {
          $svg = preg_replace('/ xmlns:' . $ns . '=".+?"/i', '', $svg);
        }
      }
    }

    $this->setContent($svg);
  }

  /**
   * Remove the ViewBox attribute if it's got the same dimensions as the height
   * and width.
   */
  private function removeViewBox() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    $elems = $xpath->query('@viewBox');

    // Get all elements with ViewBox attributes.
    if ($elems->length > 0) {
      foreach ($elems as $node) {

        // Get the integer values of the height and width.
        $height = str_replace('px', '', $node->parentNode->getAttribute('height'));
        $width = str_replace('px', '', $node->parentNode->getAttribute('width'));

        if ($height > 0 && $width > 0) {
          $values = preg_split('/[ ,]/', $node->value);

          /**
           * If the viewBox attribute has the same width and height set in it,
           * remove it.
           */
          if (!empty($values) && $width == $values[2] && $height == $values[3]) {
            $node->parentNode->removeChild($node);
          }
        }
      }
    }

    $this->setXML($xml);
  }

  /**
   * Convert style attributes to HTML element attributes.
   *
   * TODO - Refactor to use xpath?
   */
  private function convertStyleToAttrs() {
    $svg = $this->svg_file['content'];

    preg_match_all('/style=\"(.+?)\"/', $svg, $matches);

    if (!empty($matches)) {
      foreach ($matches[1] as $i => $styles) {
        $styles = explode(';', $styles);

        /**
         * Fill a new array of HTML style attributes and remove them from the
         * original HTML style attribute.
         */
        $html_style_attrs = array();
        foreach ($styles as $x => $style) {
          $attr = explode(':', $style);
          if (in_array($attr[0], $this->styleAttrs)) {
            $html_style_attrs[trim($attr[0])] = '"' . trim($attr[1]) . '"';
            unset($styles[$x]);
          }
        }

        // Make the new string of HTML attributes from the styles.
        $html_style_attrs = urldecode(http_build_query($html_style_attrs, NULL, ' '));

        /**
         * Replace the original styles attribute with the new attribute, unless
         * it's empty.
         */
        $replacement_string = $html_style_attrs;
        if (!empty($styles)) {
          $styles = implode(';', $styles);
          $replacement_string .= ' style="' . $styles . '"';
        }

        /**
         * Replace the original style attribute with the new HTML attribute
         * string and the remaining styles.
         */
        $svg = str_replace($matches[0][$i], $replacement_string, $svg);
      }

      $this->setContent($svg);
    }
  }

  /**
   * Minify rgb() and hex colors in the SVG file.
   */
  private function minifyColors() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    $elems = $xpath->query('//@color | //@fill | //@stroke | //@stop-color | //@flood-color | //@lighting-color');

    if ($elems->length > 0) {
      foreach ($elems as $node) {
        $value = $node->value;

        // Convert long color names to their hex values.
        if (isset($this->colorsNames[$value])) {
          $value = $this->colorsNames[$value];
        }

        // Convert RGB to long hex.
        if (preg_match('/rgb\(\s*([\d\.]+%?),([\d\.]+%?),([\d\.]+%?)\s*\)/i', $value, $matches)) {
          list(, $r, $g, $b) = $matches;
          $value = $this->rgbToHex($r, $g, $b);
        }

        // Hex colors.
        if (preg_match('/(\#[\da-fA-F]{6}|\#[\da-fA-F]{3})/', $value)) {
          // Convert to short hex (#000000 -> #000).
          $value = preg_replace('/#([\da-f])\1([\da-f])\2([\da-f])\3/i', '#$1$2$3', $value);

          // Use the color name, if applicable.
          if (isset($this->colorsShortNames[$value])) {
            $value = $this->colorsShortNames[$value];
          }
        }

        $node->parentNode->setAttribute($node->name, $value);
      }
    }

    $this->setXML($xml);
  }

  /**
   * Clean up lists of numbers and round to 3 decimal places.
   *
   * Depends on:
   * - roundNumberWithUnits helper
   */
  private function cleanupListOfValues() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    foreach ($this->numberAttrs as $attr) {
      $elems = $xpath->query('@' . $attr);

      if ($elems->length > 0) {
        foreach ($elems as $node) {
          /**
           * Get all of the numbers in the value and round them all to 3 decimal
           * points.
           */
          $values = array_map(array($this, 'roundNumberWithUnits'), preg_split('/[ ,]/', $node->value));

          $node->value = implode(' ', $values);
        }
      }
    }

    $this->setXML($xml);
  }

  /**
   * Clean up individual number values and round to 3 decimal places.
   *
   * Depends on:
   * - roundNumberWithUnits helper
   */
  private function cleanupNumericValues() {
    $svg = $this->svg_file['content'];

    // Find all numeric values that have 4+ decimal points to be rounded.
    preg_match_all('/"([\-+]?\d*\.\d{4,}([eE][\-+]?\d+)?)(px|pt|pc|mm|cm|m|in|ft|em|ex|%)?"/', $svg, $matches);

    if (!empty($matches[0])) {
      $values = array_map(array($this, 'roundNumberWithUnits'), $matches[1]);

      foreach ($matches[0] as $pos => $match) {
        $svg = preg_replace("/$match/", '"' . $values[$pos] . '"', $svg);
      }
    }

    $this->setContent($svg);
  }

  /**
   * Remove the enable-background attribute if height and width are supplied.
   */
  private function cleanupEnableBackground() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    // Get all elements with enable-background attributes.
    $elems = $xpath->query('@enable-background');

    if ($elems->length > 0) {
      foreach ($elems as $node) {

        // Get the integer values of the height and width.
        $height = str_replace('px', '', $node->parentNode->getAttribute('height'));
        $width = str_replace('px', '', $node->parentNode->getAttribute('width'));

        if ($height > 0 && $width > 0) {
          preg_match('/^new\s0\s0\s([\-+]?\d*\.?\d+([eE][\-+]?\d+)?)\s([\-+]?\d*\.?\d+([eE][\-+]?\d+)?)$/', $node->value, $values);

          /**
           * If the enable-background attribute has the same width and height set
           * in it and it's in the svg element, remove it.
           */
          if (!empty($values) && $width == $values[1] && $height == $values[3]) {
            if ($node->parentNode->nodeName == 'svg') {
              $node->parentNode->removeChild($node);
            }
            else {
              $node->value = 'new';
            }
          }
        }
      }
    }

    $this->setXML($xml);
  }

  /**
   * Remove empty attributes from the SVG.
   */
  private function removeEmptyAttributes() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    // Get all empty attributes.
    $elems = $xpath->query("//@*[.='']");

    if ($elems->length > 0) {
      foreach ($elems as $node) {
        $node->parentNode->removeChild($node);
      }
    }

    $this->setXML($xml);
  }

  /**
   * Remove hidden elements with disabled rendering:
   * - display="none"
   * - opacity="0"
   * - circle with zero radius
   * - ellipse with zero x-axis or y-axis radius
   * - rectangle with zero width or height
   * - pattern with zero width or height
   * - image with zero width or height
   * - path with empty data
   * - polyline with empty points
   * - polygon with empty points
   */
  private function removeHiddenElements() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    $hidden_elems = $xpath->query("//*[@display='none'] | //*[@opacity='0'] | //*[name() = 'circle'][@r='0'] | //*[name() = 'ellipse'][@rx='0' or @ry='0'] | //*[name() = 'rect' or name() = 'pattern' or name() = 'image'][@width='0' or @height='0'] | //*[name() = 'path'][@d=''] | //*[name() = 'polyline' or name() = 'polygon'][@points='']");

    if ($hidden_elems->length > 0) {
      foreach ($hidden_elems as $node) {
        $node->parentNode->removeChild($node);
      }
    }

    $this->setXML($xml);
  }

  /**
   * Remove implicit attributes.
   * Equivalent: https://github.com/svg/svgo/blob/master/plugins/removeUnknownsAndDefaults.js
   *
   * TODO - WIP
   */
  private function removeUnknownsAndDefaults() {
    $xml = $this->svg_file['xml'];

//    foreach ($xml->getElementsByTagName('*') as $elem) {
//
//    }

    $this->setXML($xml);
  }

  /**
   * Remove stroke and fill attributes that add no value.
   * Equivalent: https://github.com/svg/svgo/blob/master/plugins/removeUselessStrokeAndFill.js
   *
   * TODO - WIP
   */
  private function removeUselessStrokeAndFill() {
    $xml = $this->svg_file['xml'];

//    foreach ($xml->getElementsByTagName('*') as $elem) {
//
//    }

    $this->setXML($xml);
  }

  /**
   * TODO - WIP
   */
  private function removeUnneededGroupAttrs() {

  }

  /**
   * Convert absolute Path to relative, collapse repeated instructions, detect
   * and convert Lineto shorthands, remove useless instructions like "l0,0",
   * trim useless delimiters and leading zeros, and round floats.
   * Equivalent: https://github.com/svg/svgo/blob/master/plugins/convertPathData.js
   *
   * TODO - WIP
   */
  private function convertPathData() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    $elems = $xpath->query('//@d');

    $threshold = 2.5;
    $tolerance = 0.5;
    $precision = 3;
    $t_precision = 5;

    if ($elems->length > 0) {
      foreach ($elems as $node) {

        $path_data = $this->extractPathData($node->nodeValue);

        if (!empty($path_data)) {
          foreach ($path_data as $i => $item) {

            // Relative instructions.
            if (preg_match('/[mcslqta]/', $item)) {

            }

          }
        }

        $stop = '';

        /*$value = str_replace(' 0,0 ', ' s ', $node->nodeValue);
        $new_value = preg_split('/[\s,]/', $value);

        $new_value[0] = $value[0];

        $new_value = array_map(function($v, $i) {
          if (is_numeric($v)) {
            $v = round($v, 4);

            if ($v != 0) {
              $v = str_replace('-0.', '-.', ltrim($v, '0'));
            }

            if (($v > 1 || $v == 0) && $i != 1) {
              $v = ' ' . $v;
            }
          }

          return $v;
        }, $new_value, array_keys($new_value));

        $node->parentNode->setAttribute('d', implode('', $new_value));*/
      }
    }

    $this->setXML($xml);
  }

  /**
   * Merge path data for paths that otherwise have the same functionality.
   * Equivalent: https://github.com/svg/svgo/blob/master/plugins/mergePaths.js
   *
   * TODO - WIP
   */
  private function mergePaths() {
    $xml = $this->svg_file['xml'];

//    foreach ($xml->getElementsByTagName('*') as $elem) {
//
//    }

    $this->setXML($xml);
  }

  /**
   * Remove groups that are either empty or have attributes that can be sent to
   * child elements.
   */
  private function collapseUselessGroups() {
    $xml = $this->svg_file['xml'];
    $doc = $xml->documentElement;

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    // Get all group tags.
    $groups = $doc->getElementsByTagName('g');

    if ($groups->length > 0) {
      $group_offset = 0;
      while ($group = $groups->item($group_offset)) {

        if ($group->hasChildNodes() && $group->childNodes->length > 1) {
          $group_offset++;
          continue;
        }

        // Select child nodes that are not empty text.
        if ($group->hasAttributes() && $group->hasChildNodes() && $group->childNodes->length == 1) {

          // Move the inheritable attributes to the child item.
          $attribute_offset = 0;
          while ($attr = $group->attributes->item($attribute_offset)) {

            if (in_array($attr->name, $this->inheritableAttrs)) {
              $value = $attr->value;

              // Append transform values.
              if ($attr->name == 'transform' && $group->childNodes->item(0)->hasAttribute($attr->name)) {
                $value .= ' ' . $group->childNodes->item(0)->getAttribute($attr->name);
              }

              $group->childNodes->item(0)->setAttribute($attr->name, $value);
              $group->removeAttribute($attr->name);
            }
            else {
              $attribute_offset++;
              continue;
            }
          }

          // If this group no longer has any attributes, move the childNode up one level and remove the group.
          if (!$group->hasAttributes()) {
            $group->parentNode->appendChild($group->childNodes->item(0));
            $group->parentNode->removeChild($group);
          }
        }
      }
    }

    $this->setXML($xml);
  }

  /**
   * Combine, simplify and round transform functions.
   */
  private function cleanupTransforms() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    $elems_with_transform = $xpath->query("//*[@transform] | //*[@gradientTransform] | //*[@patternTransform]");

    if ($elems_with_transform->length > 0) {
      foreach ($elems_with_transform as $node) {

        // Certain tags expect certain transform-type attributes.
        switch($node->nodeName) {
          case 'radialGradient':
          case 'linearGradient':
            $attr_name = 'gradientTransform';
            break;

          case 'pattern':
            $attr_name = 'patternTransform';
            break;

          default:
            $attr_name = 'transform';
            break;
        }

        $value = $node->getAttribute($attr_name);

        if (empty($value)) {
          continue;
        }

        preg_match_all('/(\w+?)\((.+?)\)/', $value, $matches);

        // Combine all function values of the same name.
        $functions = array();
        foreach ($matches[1] as $key => $function) {
          $value = $matches[2][$key];

          switch($function) {
            case 'translate':
              if (!isset($functions['translate'])) {
                $functions['translate'] = array_fill(0, 2, 0);
              }

              list($x, $y) = preg_split('/[\s,]/', $value);
              $functions['translate'][0] += $x;
              $functions['translate'][1] += (!empty($y) ? $y : $x);
              break;

            case 'rotate':
              if (!isset($functions['rotate'])) {
                $functions['rotate'] = array_fill(0, 3, 0);
              }

              list($a, $x, $y) = preg_split('/[\s,]/', $value);
              $functions['rotate'][0] += $a;
              $functions['rotate'][1] += $x;
              $functions['rotate'][2] += (!empty($y) ? $y : $x);
              break;

            case 'skewX':
              $functions['skewX'] += $value;
              break;

            case 'skewY':
              $functions['skewY'] += $value;
              break;

            case 'matrix':
              $value = preg_split('/[\s,]/', $value);

              if (!isset($functions['matrix'])) {
                $functions['matrix'] = $value;
              }
              else {
                $functions['matrix'] = $this->combineMatrices($functions['matrix'], $value);
              }
              break;

            case 'scale':
              if (!isset($functions['scale'])) {
                $functions['scale'] = array_fill(0, 2, 0);
              }

              list($x, $y) = preg_split('/[\s,]/', $value);
              $functions['scale'][0] += $x;
              $functions['scale'][1] += (!empty($y) ? $y : $x);
              break;
          }
        }

        // Clean up and simplify the combined functions and remove useless ones.
        if ($functions['scale'][0] == $functions['scale'][1]) {
          if ($functions['scale'][0] > 1) {
            $functions['scale'] = $functions['scale'][0];
          }
          else {
            unset($functions['scale']);
          }
        }

        if (isset($functions['rotate']) && isset($functions['translate'])) {
          // Move cx and cy to translate.
          if ($functions['rotate'][0] == 0) {
            $functions['translate'][0] += $functions['rotate'][1];
            $functions['translate'][1] += $functions['rotate'][2];
            unset($functions['rotate']);
          }
          // Move cx and cy to rotate if the rotate values are not 0.
          else {
            $functions['rotate'][1] += $functions['translate'][0];
            $functions['rotate'][2] += $functions['translate'][1];
            unset($functions['translate']);
          }
        }

        // Skew functions are useless if they are 0.
        if (isset($functions['skewX']) && $functions['skewX'] == 0) {
          unset($functions['skewX']);
        }
        if (isset($functions['skewY']) && $functions['skewY'] == 0) {
          unset($functions['skewY']);
        }

        /**
         * Remove matrices that are just 1 0 0 1 0 0, because they are also
         * useless.
         */
        if (isset($functions['matrix']) && $functions['matrix'][0] == 1 && $functions['matrix'][3] == 1 && !($functions['matrix'][1] || $functions['matrix'][2] || $functions['matrix'][4] || $functions['matrix'][5])) {
          unset($functions['matrix']);
        }

        $new_value = [];
        foreach ($functions as $func => $value) {
          // Round all of the numbers to 1 decimal point.
          $value = array_map(function($n) {
            return round($n,1);
          }, $value);

          if (is_array($value)) {
            $value = implode(' ', $value);
          }

          $new_value[] = "$func($value)";
        }

        $new_value = implode(' ', $new_value);
        $node->setAttribute($attr_name, $new_value);
      }
    }

    $this->setXML($xml);
}

  /**
   * Remove empty a, defs, g, marker, mask, missing-glyph, pattern, svg, switch,
   * symbol, and foreignObject tags.
   * Equivalent: https://github.com/svg/svgo/blob/master/plugins/removeEmptyContainers.js
   *
   * TODO - WIP
   */
  private function removeEmptyContainers() {

  }

  /**
   * Sort SVG element attributes into a specific order.
   */
  private function sortAttributes() {
    $xml = $this->svg_file['xml'];

    // Loop through all of the elements in the document.
    foreach ($xml->getElementsByTagName('*') as $elem) {

      if ($elem->hasAttributes()) {

        // Get the original attributes out and into an array that can be sorted.
        $attrs = array();
        $unordered_attrs = array();
        while ($attr = $elem->attributes->item(0)) {
          if (in_array($attr->name, $this->attrOrder)) {
            $attrs[$attr->name] = $attr->nodeValue;
          }
          else {
            $unordered_attrs[$attr->name] = $attr->nodeValue;
          }

          $elem->removeAttribute($attr->name);
        }

        $attr_order = array_flip(array_intersect_key(array_flip($this->attrOrder), $attrs));
        $new_attrs = array_combine($attr_order, $attrs) + $unordered_attrs;

        foreach($new_attrs as $name => $value) {
          $elem->setAttribute($name, $value);
        }
      }
    }

    $this->setXML($xml);

  }

  /**
   * Make sure that we include aria-hidden="true" in the <svg> tag, because
   * we're already removing the title and description automatically. We can add
   * logic later to check that the SVG contains a title and description, if
   * necessary. See the Inline SVG section at
   * https://css-tricks.com/accessible-svgs/.
   */
  private function addAriaHidden() {
    $xml = $this->svg_file['xml'];

    $xpath = new DOMXPath($xml);
    $xpath->registerNamespace('default', $this->defaultNS);

    $svg_tag = $xpath->query('/*')->item(0);
    $svg_tag->setAttribute('aria-hidden', 'true');

    $this->setXML($xml);
  }

  /**
   * Helper functions
   */

  /**
   * Make sure we're saving the SVG string content and recreating the XML from
   * it.
   *
   * @param string $content The string version of the SVG.
   */
  private function setContent($content) {
    $this->svg_file['content'] = $content;
    $this->svg_file['xml']->loadXML($content);
  }

  /**
   * Save our XML DOMDocument object and and XML string to $this->svg_file.
   * This essentially just runs DOMDocument::saveXML(), but the original
   * saveXML re-adds the <?xml> tag and the LIBXML_NOXMLDECL flag doesn't work
   * yet, so we need to work around it by saving the root <svg> element and
   * everything in it.
   *
   * @param \DOMDocument $xml
   */
  private function setXML(\DOMDocument $xml) {
    $this->svg_file['xml'] = $xml;
    $this->svg_file['content'] = $xml->saveXML($xml->getElementsByTagName('svg')->item(0));
  }

  /**
   * Round number to decimal points and make sure that if the original number
   * is in px that it's the most efficient number value.
   *
   * @param $num float The number to be rounded.
   * @param $precision int The number of decimal places to round to.
   */
  private function roundNumberWithUnits($num, $precision = 3) {
    // If the value doesn't contain any numbers at all, just send it back.
    if (!preg_match('/\d*/', $num)) {
      return $num;
    }

    // Get the unit string from the $num value.
    preg_match('/^([\-+]?\d*\.?\d+([eE][\-+]?\d+)?)(px|pt|pc|mm|cm|m|in|ft|em|ex|%)?$/', $num, $matches);

    // Round the number regardless of its unit.
    $num = round($matches[1], $precision);

    // Remove the default pixels unit.
    $unit = '';
    if (isset($matches[3]) && $matches[3] != 'px') {
      $unit = $matches[3];
    }

    /**
     * If the actual pixel value string is shorter, use it and keep the
     * $unit string empty.
     */
    if (!empty($unit) && isset($this->absoluteLengths[$unit])) {
      $pxNum = round($this->absoluteLengths[$unit] * $matches[1], 3);

      if (strlen($pxNum) < strlen($matches[0])) {
        $num = $pxNum;
      }
    }

    /**
     * Remove leading zeroes, re-add the unit and send it back to the
     * original attribute value.
     */
    $num = preg_replace('/^0*?([1-9]*?)\./', '$1.', $num);
    return $num . $unit;
  }

  /**
   * Combine 2 matrix() array values.
   *
   * @param $m1 array The first matrix array.
   * @param $m2 array The second matrix array.
   * @return array The combined matrix.
   */
  private function combineMatrices($m1, $m2) {
    return array(
      $m1[0] * $m2[0] + $m1[2] * $m2[1],
      $m1[1] * $m2[0] + $m1[3] * $m2[1],
      $m1[0] * $m2[2] + $m1[2] * $m2[3],
      $m1[1] * $m2[2] + $m1[3] * $m2[3],
      $m1[0] * $m2[4] + $m1[2] * $m2[5] + $m1[4],
      $m1[1] * $m2[4] + $m1[3] * $m2[5] + $m1[5]
    );
  }

  /**
   * Split up path data for cleanup and rounding tasks.
   * @param $path string The original path data from the SVG.
   * @return array
   */
  private function extractPathData($path) {
    $instructions = '[MmLlHhVvCcSsQqTtAaZz]';

    $path = array_filter(preg_split("/($instructions)\s*/", $path, null, PREG_SPLIT_DELIM_CAPTURE));
    $path_data = array();

    while (count($path) >= 1) {
      if (in_array($path[0], array('Z', 'z'))) {
        $instruction = array_shift($path);
        $data = null;
      }
      else {
        list($instruction, $data) = array_splice($path, 0, 2);
      }

      $path_data[] = array(
        'instruction' => (in_array($instruction, array('M', 'm')) && count($path_data) == 0 ? 'M' : $instruction),
        'data'=> $data,
      );
    }

    return $path_data;
  }

  /**
   * Convert rgb color value to hex value.
   *
   * @param $r int Red value.
   * @param $g int Green value.
   * @param $b int Blue value.
   */
  private function rgbToHex($r, $g, $b) {
    // Convert percentages to n/255.
    $is_percent = '/^([\d\.]+?)%$/';
    if (preg_match($is_percent, $r, $pct)) {
      $r = ceil(($pct[1]/100) * 255);
    }
    if (preg_match($is_percent, $g, $pct)) {
      $g = ceil(($pct[1]/100) * 255);
    }
    if (preg_match($is_percent, $b, $pct)) {
      $b = ceil(($pct[1]/100) * 255);
    }

    return sprintf("#%02x%02x%02x", $r, $g, $b);
  }

}
