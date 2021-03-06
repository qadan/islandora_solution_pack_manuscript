<?php

/**
 * @file
 * Theme hooks.
 */

define('ISLANDORA_MANUSCRIPT_CONTAINER_TAG_URI', 'http://islandora.ca/manuscript-container');

/**
 * Implements hook_preprocess_theme().
 */
function template_preprocess_islandora_manuscript_manuscript(array &$variables) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_manuscript', 'includes/utilities');
  $object = $variables['object'];
  $params = array(
    'object' => $object,
    'pages' => islandora_paged_content_get_pages($object),
    'page_progression' => islandora_paged_content_get_page_progression($object),
    'transform_object' => islandora_manuscript_get_manuscript_transform_object($object),
  );
  module_load_include('inc', 'islandora', 'includes/solution_packs');
  $viewer = islandora_get_viewer($params, 'islandora_manuscript_viewers');
  if ($viewer) {
    $variables['viewer'] = $viewer;
    if (islandora_get_viewer_id('islandora_manuscript_viewers') == 'islandora_internet_archive_bookreader') {
      // Add in our IA class if applicable.
      // Create the class array here so we don't get an empty attribute.
      $variables['viewer_wrapper_attributes']['class'] = array();
      $variables['viewer_wrapper_attributes']['class'][] = 'ia-bookreader';
    }
  }

  $variables['islandora_manuscript_metadata'] = variable_get('islandora_manuscript_metadata_display', FALSE);
  if ($variables['islandora_manuscript_metadata']) {
    drupal_add_js('misc/form.js');
    drupal_add_js('misc/collapse.js');
    module_load_include('inc', 'islandora', 'includes/metadata');
    $variables['parent_collections'] = islandora_get_parents_from_rels_ext($object);
    $variables['metadata'] = islandora_retrieve_metadata_markup($object);
    $variables['description'] = islandora_retrieve_description_markup($object);
  }
}

/**
 * Implements hook_preprocess_theme().
 */
function template_preprocess_islandora_manuscript_page(array &$variables) {
  module_load_include('inc', 'islandora', 'includes/solution_packs');
  $object = $variables['object'];
  $results = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOf');
  $result = reset($results);
  $variables['manuscript_object_id'] = $result ? $result['object']['value'] : FALSE;

  $params = array();
  if (isset($object['JP2']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['JP2'])) {
    // Get token to allow access to XACML protected datastreams.
    // Always use token authentication in case there is a global policy.
    module_load_include('inc', 'islandora', 'includes/authtokens');
    $token = islandora_get_object_token($object->id, 'JP2', 2);
    $jp2_url = url("islandora/object/{$object->id}/datastream/JP2/view", array(
      'absolute' => TRUE,
      'query' => array('token' => $token),
    ));
    $params['token'] = $token;
    $params['pid'] = $object->id;
    $params['dsid'] = 'JP2';
    // Can be removed after 7.x-1.11 is out the door islandora_deprecated.
    $params['jp2_url'] = $jp2_url;
  }

  $viewer = islandora_get_viewer($params, 'islandora_manuscript_page_viewers');
  if ($viewer) {
    $variables['viewer'] = $viewer;
  }
}

/**
 * Prepares variables for islandora_manuscript_ead_display templates.
 *
 * Default template: islandora-manuscript-ead-display.tpl.php
 *
 * @param array $variables
 *   An associative array containing:
 *   - object: An AbstractObject containing an "EAD" datastream.
 *   - xslt_functions: An array of functions to allow the XSLT to run, as
 *     accepted by XSLTProcessor::registerPhpFunctions().
 *   - xslt_parameters: An associative array mapping namespace URIs to
 *     associative arrays of parameters proper.
 *   This function populates:
 *   - doc: A DOMDocument containing the parsed EAD datastream.
 *   - xslt_doc: A DOMDocument containing the parsed XSLT to run.
 */
function template_preprocess_islandora_manuscript_ead_display(&$variables) {
  $variables['xslt_parameters'][''] = (isset($variables['xslt_parameters']['']) ? $variables['xslt_parameters'][''] : array()) + array(
    "container_string" => t('Containers'),
  );
  $variables['doc'] = $doc = new DOMDocument();
  $doc->loadXML($variables['object']['EAD']->content);

  // XXX: Need to tag containers in order to work-around a PHP bug. See
  // islandora_manuscript_lookup_tag() for more details on the bug.
  // This _could_ be wrapped in version checks, so we only tag when necessary.
  islandora_manuscript_tag_containers($doc);

  $variables['xslt_doc'] = $xslt_doc = new DOMDocument();
  $xslt_doc->load(drupal_get_path('module', 'islandora_manuscript') . '/transforms/ead_to_html.xslt');

  // Add JS for fieldsets.
  drupal_add_js('misc/form.js');
  drupal_add_js('misc/collapse.js');
}

/**
 * Tag containers with a unique ID.
 *
 * Part of a work around for a PHP bug in which nodesets passed out of XSLTs
 * are copied.
 *
 * @param DOMDocument $doc
 *   A DOMDocument containing a parsed EAD document, in which we will tag all
 *   containers with a document-unique attribute.
 */
function islandora_manuscript_tag_containers(DOMDocument $doc) {
  $xpath = new DOMXPath($doc);
  $xpath->registerNamespace('ead', 'urn:isbn:1-931666-22-9');
  foreach ($xpath->query('//ead:container') as $index => $container) {
    $container->setAttributeNS(ISLANDORA_MANUSCRIPT_CONTAINER_TAG_URI, 'container-tag:id', "islandora-manuscript-container-tag:$index");
  }
}

/**
 * Process variables for islandora_manuscript_ead_display templates.
 *
 * Default template: islandora-manuscript-ead-display.tpl.php
 *
 * @param array $variables
 *   An associative array containing:
 *   - object: An AbstractObject containing an "EAD" datastream.
 *   - xslt_functions: An array of functions to allow the XSLT to run, as
 *     accepted by XSLTProcessor::registerPhpFunctions().
 *   - xslt_parameters: An associative array mapping namespace URIs to
 *     associative arrays of parameters proper.
 *   - doc: A DOMDocument containing the parsed EAD datastream.
 *   - xslt_doc: A DOMDocument containing the parsed XSLT to run.
 *   This function populates:
 *   - processor: The XSLTProcessor instance which was used.
 *   - markup_doc: A DOMDocument containing the markup to output, after
 *     this function has run.
 */
function template_process_islandora_manuscript_ead_display(&$variables) {
  $variables['processor'] = $proc = new XSLTProcessor();
  $proc->importStylesheet($variables['xslt_doc']);
  foreach ($variables['xslt_parameters'] as $namespace_uri => $parameters) {
    $proc->setParameter($namespace_uri, $parameters);
  }
  $proc->registerPhpFunctions($variables['xslt_functions']);
  $variables['markup_doc'] = $proc->transformToDoc($variables['doc']);
}

/**
 * Callback used in XSLT to build a query URL.
 *
 * @param DOMElement[] $container_array
 *   An array containing a single DOMElement (this is how XSLTProcessor
 *   provides it) representing a "container" inside of an EAD document.
 * @param DOMElement[] $all
 *   An array containing all container elements in the given EAD document.
 *
 * @return string
 *   A string containing a URL to Solr search results for the given container.
 */
function islandora_manuscript_build_parented_query_url(array $container_array, array $all) {
  $object = menu_get_object('islandora_object', 2);
  if ($object) {
    $path = "islandora/object/{$object->id}/manuscript/manuscripts";
    list($container) = $container_array;
    return url($path, array(
      'query' => array(
        'f' => islandora_manuscript_build_subfile_query($container, $all),
      ),
    ));
  }
}

/**
 * Build a query to filter to the given container.
 *
 * @param DOMElement $container
 *   A DOMElement representing the container.
 *
 * @return string[]
 *   An array of strings representing Lucene queries... Likely to be used as
 *   filter queries.
 */
function islandora_manuscript_build_partial_query(DOMElement $container) {
  $subqueries = array();
  $field = islandora_manuscript_findingaid_get_solr_field($container->getAttribute('type'));

  $value = trim($container->nodeValue);
  if ($value != '') {
    $subqueries[] = format_string('!field:"!value"', array(
      '!field' => $field,
      '!value' => $value,
    ));
  }

  return $subqueries;
}

/**
 * Build a query to select all items in a given part of a file.
 *
 * @param DOMElement $container
 *   An EAD container element for which to build a (part of a) query.
 * @param DOMElement[] $all
 *   An array of all container elements in the EAD doc...  'Cause the "parent"
 *   attribute can reference any container element.
 *
 * @return string[]
 *   An array of Lucene-syntax Solr queries.
 */
function islandora_manuscript_build_subfile_query(DOMElement $container, array $all = array()) {
  $subqueries = islandora_manuscript_build_partial_query($container);

  if ($container->hasAttribute('parent')) {
    foreach ($all as $element) {
      if ($element->getAttribute('id') == $container->getAttribute('parent')) {
        $subqueries = array_merge(islandora_manuscript_build_subfile_query($element, $all), $subqueries);
      }
    }
  }

  $component = islandora_manuscript_get_container_component($container);
  if ($component && $component->hasAttribute('id')) {
    $subqueries[] = format_string('!field:"!value"', array(
      '!field' => variable_get('islandora_manuscript_component_identifier_solr_field', 'dereffed_ead_component_id_ms'),
      '!value' => $component->getAttribute('id'),
    ));
  }

  return $subqueries;
}

/**
 * Get the component to which the given container belongs.
 *
 * @param DOMElement $container
 *   A container element.
 *
 * @return DOMElement|bool
 *   The parent component if we could find it; otherwise, FALSE.
 */
function islandora_manuscript_get_container_component(DOMElement $container) {
  $concrete_container = isset($container->parentNode) ?
    $container :
    islandora_manuscript_lookup_tag($container);

  return $concrete_container ?
    $concrete_container->parentNode->parentNode :
    FALSE;
}

/**
 * Use our "tag" ID to look up the concrete container.
 *
 * Certain versions of PHP provide element copies lacking references to parent
 * elements. To work around this, we may have "tagged" each container with a
 * attribute, which we can use to get back to the "real" element from which it
 * was copied.
 *
 * @param DOMElement $container
 *   A container element to lookup.
 *
 * @return DOMElement|bool
 *   The container if we could find it; otherwise, FALSE.
 *
 * @see https://github.com/php/php-src/commit/6408a1a59e6d371cd488687e28e18815ea97984e#diff-258cc1cabc37df15d7f0ed40924f64efR283
 */
function islandora_manuscript_lookup_tag(DOMElement $container) {
  $tag = $container->getAttributeNS(ISLANDORA_MANUSCRIPT_CONTAINER_TAG_URI, 'id');
  $xpath = new DOMXPath($container->ownerDocument);
  $xpath->registerNamespace('ead', 'urn:isbn:1-931666-22-9');
  $xpath->registerNamespace('container-tag', ISLANDORA_MANUSCRIPT_CONTAINER_TAG_URI);
  $results = $xpath->query("//ead:container[@container-tag:id='$tag']");

  return $results->length > 0 ?
    $results->item(0) :
    FALSE;
}

/**
 * Get the field for the given "type" of container.
 *
 * @param string $raw_type
 *   The raw type attribute value from the XML. A number of different formats
 *   have been seen in the wild, with boxes, for example:
 *   - Box
 *   - Boxes
 *   - box
 *   - boxes
 *   As a naive implementation, we lowercase and then compare at the beginning
 *   of the string for one of our recognized types, currently, just:
 *   - box
 *   - folder
 *
 * @return string|bool
 *   A string containing the name of a Solr field with which a query might be
 *   built, or FALSE if we do not have a mapping.
 */
function islandora_manuscript_findingaid_get_solr_field($raw_type) {
  $type = strtolower($raw_type);
  if (strpos($type, 'box') === 0) {
    return variable_get('islandora_manuscript_box_identifier_solr_field', 'mods_relatedItem_host_part_detail_box_number_ms');
  }
  elseif (strpos($type, 'folder') === 0) {
    return variable_get('islandora_manuscript_folder_identifier_solr_field', 'mods_relatedItem_host_part_detail_folder_number_ms');
  }
  else {
    watchdog('islandora_manuscript', 'Unrecognized type @type.', array('@type' => $raw_type));
    return FALSE;
  }
}

/**
 * Callback used in XSLT to build a query URL.
 *
 * @param DOMElement[] $containers
 *   An array containing a single DOMElement (this is how XSLTProcessor
 *   provides it) representing a "container" inside of an EAD document.
 *
 * @return string
 *   A string containing a URL to Solr search results for the given container.
 */
function islandora_manuscript_build_flat_query_url(array $containers) {
  $object = menu_get_object('islandora_object', 2);
  $parts = islandora_manuscript_build_flat_subfile_query($containers);
  if ($object && !empty($parts)) {
    $path = "islandora/object/{$object->id}/manuscript/manuscripts";
    return url($path, array(
      'query' => array(
        'f' => $parts,
      ),
    ));
  }
}

/**
 * Helper function to wrap the map and merge.
 *
 * @param DOMElement[] $containers
 *   An array of containers at the same component level in the EAD.
 *
 * @return string[]
 *   An array of strings representing Lucene queries.
 */
function islandora_manuscript_build_flat_subfile_query(array $containers) {
  // Array of array of Lucence queries...
  $parts = array_map('islandora_manuscript_build_partial_query', $containers);

  // Merge down to single array.
  return empty($parts) ? array() : call_user_func_array('array_merge', $parts);
}
