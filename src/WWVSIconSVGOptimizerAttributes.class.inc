<?php
/**
 * @file WWVSIconSVGOptimizerAttributes class.
 */

/**
 * Class WWVSIconSVGOptimizerAttributes
 *
 * Add custom attributes to the list of allowed attributes.
 */
class WWVSIconSVGOptimizerAttributes implements enshrined\svgSanitize\data\AttributeInterface {

  public static $filter_attributes = array();
  public static $custom_attributes = array();

  /**
   * Returns an array of attributes
   *
   * @return array
   */
  public static function getAttributes() {
    $allowed = enshrined\svgSanitize\data\AllowedAttributes::getAttributes();
    $allowed = array_unique(array_merge($allowed, static::$custom_attributes));
    $allowed = array_diff($allowed, static::$filter_attributes);

    return $allowed;
  }

  /**
   * Set the attributes to be filtered out of the allowed array.
   *
   * @param $filter_attributes array An array of tags to be filtered out of the
   *  list of allowed tags.
   */
  public function removeAttributes($filter_attributes) {
    static::$filter_attributes = $filter_attributes;
  }

  /**
   * Set the attributes to be added to the allowed array.
   *
   * @param $custom_attributes array An array of attributes to be added in to
   * the list of allowed attributes.
   */
  public function addAttributes($custom_attributes) {
    static::$custom_attributes = $custom_attributes;
  }
}
