<?php
/**
 * @file WWVSIconSVGOptimizerTags class.
 */

/**
 * Class WWVSIconSVGOptimizerTags
 *
 * Add custom attributes to the list of allowed attributes.
 */
class WWVSIconSVGOptimizerTags implements enshrined\svgSanitize\data\TagInterface {

  public static $filter_tags = array();
  public static $custom_tags = array();

  /**
   * Returns an array of tags
   *
   * @return array
   */
  public static function getTags() {
    $allowed = enshrined\svgSanitize\data\AllowedTags::getTags();
    $allowed = array_unique(array_merge($allowed, static::$custom_tags));
    $allowed = array_diff($allowed, static::$filter_tags);

    return $allowed;
  }

  /**
   * Set the tags to be filtered out of the allowed array.
   *
   * @param $filter_tags array An array of tags to be filtered out of the list
   *  of allowed tags.
   */
  public function removeTags($filter_tags) {
    static::$filter_tags = $filter_tags;
  }

  /**
   * Set the tags to be added to the allowed array.
   *
   * @param $custom_tags array An array of tags to be added in to the list of
   *  allowed tags.
   */
  public function addTags($custom_tags) {
    static::$custom_tags = $custom_tags;
  }
}
