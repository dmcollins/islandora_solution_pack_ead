<?php

/**
 * @file
 * Handles the display/submission for uploading associated objects for an EAD.
 *
 * This file is part of the Islandora EAD Solution Pack.
 * Copyright (C) 2015  Drexel University.
 *
 * The Islandora EAD Solution Pack is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * The Islandora EAD Solution Pack is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with The Islandora EAD Solution Pack; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * Defines a file upload form for uploading an associated file.
 *
 * @param array $form
 *   The drupal form.
 * @param array $form_state
 *   The drupal form state.
 * @param AbstractObject $ead
 *   The object representing the EAD.
 * @param string $refid
 *   ID of the level in the EAD content inventory being modified.
 *
 * @return array
 *   The drupal form definition.
 */
function islandora_ead_association_modify_form(array $form, array &$form_state, AbstractObject $ead, $refid) {


  $form['eadid'] = array(
    '#type' => 'submit',
    '#value' => $ead->id,
  );

  $form['refid'] = array(
    '#type' => 'submit',
    '#value' => $refid,
  );

  $header = array(
    'level' => t('Level'),
    'info' => t('Info'),
    'association' => t('Association'),
  );

  $xsl = new DOMDocument();
  $xsl->load(drupal_get_path('module', 'islandora_ead') . '/xml/getEadLevels.xsl');

  $xslt = new XSLTProcessor();
  $xslt->importStyleSheet($xsl);

  module_load_include('inc', 'islandora_ead', 'includes/utilities');
  $associations = islandora_ead_retrieve_associations($ead->id);

  $form['associate'] = array(
     '#type' => 'submit',
     '#value' => $associations,
  );

  $ead_dom = new DOMDocument();
  $ead_dom->loadXML($ead['EAD']->content);
  $ead_dom = $xslt->transformToDoc($ead_dom);

  $partsxml = new SimpleXMLElement($ead_dom->saveXML());

  $parts_idx = 1;
  $parts = array();
  
  foreach ($partsxml->entity as $entity) {
    $info = $entity->title;
if ($entity->id == $refid) {

    if (trim($entity->extent) !== '') {
      $info .= "<br/>$entity->extent";
    }
    if (trim($entity->desc) !== '') {
      $info .= "<br/>$entity->desc";
    }

    $containers = array();
    foreach ($entity->container as $container) {
      if (trim($container->type) !== '') {
        $containers[] = "$container->type: $container->value";
      }
    }

    if ($containers) {
      $info .= "<br/>" . implode(", ", $containers);
    }

    $entity_id = (string) $entity->id;
    $association = '';

    if (array_key_exists($entity_id, $associations)) {
      $association = $associations[$entity_id];
      $assoc_link = "<a href='/islandora/object/$association'>$association</a>";
    }
    else {
      $assoc_link = "No Associations";
    }

    $parts[$parts_idx++] = array(
      'level' => $entity->level,
      'info' => $info,
      'association' => $assoc_link,
    );

  $form['table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $parts,
    '#empty' => 'No Associations',
  );

$form['mod_assoc'] = array(
  '#type' => 'textfield',
  '#title' => t('Associated Object'),
  '#default_value' => $association,
  '#autocomplete_path' => 'islandora_ead_object/autocomplete/object',
);

return $form;
}
}

}

/**
 * AJAX callback for EAD association form.
 */
/*function islandora_ead_association_modify_form_ajax(array $form, array &$form_state) {
  return $form['upload_wrapper'];
}*/

/**
 * Submit handler, adds uploaded file to ingest object.
 */
function islandora_ead_association_modify_form_submit(array $form, array &$form_state) {

  $tuque = islandora_get_tuque_connection();

  $model = $form_state['values']['models'];
  $eadid = $form_state['values']['eadid'];
  $refid = $form_state['values']['refid'];

  // Create new object to add to repo; Assign content model and relationships.
  $object = $tuque->repository->constructObject(islandora_get_namespace($eadid));
  $object->models = $model;
  $object->relationships->add(FEDORA_RELS_EXT_URI, 'isPartOf', $eadid);
  $object->relationships->add(FEDORA_RELS_EXT_URI, 'eadLevelId', $refid);

  // Retrieve uploaded file contents and create OBJ datastream.
  $ds = $object->constructDatastream('OBJ', 'M');
  $file = file_load($form_state['values']['file_upload']);
  $path = drupal_realpath($file->uri);
  $ds->setContentFromFile($path, FALSE);
  $ds->label = $file->filename;
  $ds->mimetype = $file->filemime;
  $object->ingestDatastream($ds);

  // Create DC datastream and populate with metadata from parent EAD.
  $ds = $object->constructDatastream('DC', 'X');
  $ds->label = "Dublin Core Record for this object";
  $ds->setContentFromString(islandora_ead_generate_dc($object->id, $eadid, $refid));
  $object->ingestDatastream($ds);

  // Actually add the object to the repository.
  islandora_add_object($object);

  // Redirect browser to the newly added object.
  $form_state['redirect'] = "islandora/object/{$object->id}";
}

/**
 * Creates DC XML based on a specific subset of EAD metadata.
 *
 * @param string $objid
 *   The PID of the object the metadata applies to.
 * @param string $eadid
 *   The PID of the parent EAD object.
 * @param string $refid
 *   The id of the section in the EAD to retrieve metadata from.
 */
function islandora_ead_generate_dc($objid, $eadid, $refid) {
  $ead = islandora_object_load($eadid);
  $eadxml = new SimpleXMLElement($ead['EAD']->content);
  $level = $eadxml->archdesc->dsc->xpath("//*[@id='$refid']");

  // refid's are unique, should only be one result.
  $level = $level[0];
  $title = (string) $level->did->unittitle;

  $dcxml = <<<XML
<oai_dc:dc xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:oai_dc="http://www.openarchives.org/OAI/2.0/oai_dc/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.openarchives.org/OAI/2.0/oai_dc/ http://www.openarchives.org/OAI/2.0/oai_dc.xsd">
  <dc:title>$title</dc:title>
  <dc:identifier>$objid</dc:identifier>
</oai_dc:dc>
XML;

  return $dcxml;
}
